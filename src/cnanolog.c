/* Copyright (c) 2025
 * CNanoLog Binary Implementation
 *
 * Replaces text-based logging with binary format.
 */

#include "../include/cnanolog.h"
#include "../include/cnanolog_format.h"
#include "log_registry.h"
#include "binary_writer.h"
#include "arg_packing.h"
#include "platform.h"
#include "staging_buffer.h"
#include "compressor.h"
#include "cycles.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_LOG_ENTRY_SIZE 4096
#define MAX_STAGING_BUFFERS 256  /* Maximum number of concurrent threads */

/**
 * Batch processing configuration for background writer.
 *
 * The background writer flushes when:
 * 1. FLUSH_BATCH_SIZE entries written, OR
 * 2. FLUSH_INTERVAL_MS milliseconds elapsed, OR
 * 3. No more work found (buffer empty)
 *
 * Tuning for different scenarios:
 * - High throughput/burst: FLUSH_BATCH_SIZE=2000, INTERVAL=200ms (more buffering, fewer flushes)
 * - Low latency: FLUSH_BATCH_SIZE=10, INTERVAL=10ms (less buffering, more flushes)
 * - Balanced (default): FLUSH_BATCH_SIZE=1000, INTERVAL=100ms
 *
 * Benchmark optimized: Higher batch size to reduce flush frequency under heavy load.
 * Prevents log drops during burst scenarios where tons of logs arrive in short time.
 */
#define FLUSH_BATCH_SIZE 2000          /* Flush every N entries */
#define FLUSH_INTERVAL_MS 200          /* OR flush every N milliseconds */

/* ============================================================================
 * Global State
 * ============================================================================ */

static log_registry_t g_registry;
static binary_writer_t* g_binary_writer = NULL;
static volatile int g_should_exit = 0;
static int g_is_initialized = 0;

/* Background writer thread */
static cnanolog_thread_t g_writer_thread;

/* Timing for binary logs */
#ifndef CNANOLOG_NO_TIMESTAMPS
static uint64_t g_start_timestamp = 0;
static time_t g_start_time_sec = 0;
static int32_t g_start_time_nsec = 0;
static uint64_t g_timestamp_frequency = 0;  /* CPU frequency (Hz) for rdtsc() */
#endif

/* ============================================================================
 * Global Statistics Tracking
 * ============================================================================ */

#if !defined(CNANOLOG_NO_TIMESTAMPS) && !defined(CNANOLOG_NO_STATISTICS)
static struct {
    volatile uint64_t total_logs;            /* Logs written by threads */
    volatile uint64_t dropped_logs;          /* Logs dropped (buffer full) */
    volatile uint64_t bytes_written;         /* Bytes written to file */
    volatile uint64_t bytes_compressed_from; /* Uncompressed size */
    volatile uint64_t bytes_compressed_to;   /* Compressed size */
    volatile uint64_t background_wakeups;    /* Background thread wakeups */
} g_stats = {0, 0, 0, 0, 0, 0};
#endif

/* ============================================================================
 * Global Buffer Registry (for background thread to find all buffers)
 * ============================================================================ */

/**
 * Lock-free buffer registry using atomic operations.
 *
 * Performance optimization:
 * - Old: Mutex lock on first log from each thread (50-500ns spike)
 * - New: Atomic operations only (< 10ns overhead)
 *
 * Thread safety:
 * - count: Atomic fetch-and-add ensures unique indices
 * - buffers[i]: Atomic store with release semantics ensures visibility
 * - Background thread uses acquire semantics to see all previous writes
 */
typedef struct {
    staging_buffer_t* buffers[MAX_STAGING_BUFFERS];
    volatile uint32_t count;
} buffer_registry_t;

static buffer_registry_t g_buffer_registry;

/* ============================================================================
 * Thread-Local Storage
 * ============================================================================ */

/*
 * Each thread gets its own staging buffer.
 * Use C11 _Thread_local or __thread (GCC/Clang extension).
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    /* C11 standard */
    static _Thread_local staging_buffer_t* tls_staging_buffer = NULL;
#elif defined(__GNUC__) || defined(__clang__)
    /* GCC/Clang extension */
    static __thread staging_buffer_t* tls_staging_buffer = NULL;
#else
    #error "Thread-local storage not supported on this compiler"
#endif

/* Thread ID counter for debugging */
static volatile uint32_t g_next_thread_id = 1;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void* writer_thread_main(void* arg);
static uint64_t get_timestamp(void);
static void buffer_registry_init(buffer_registry_t* registry);
static int buffer_registry_add(buffer_registry_t* registry, staging_buffer_t* buffer);
static staging_buffer_t* get_or_create_staging_buffer(void);

/* ============================================================================
 * Timestamp Calibration (Phase 5)
 * ============================================================================ */

#ifndef CNANOLOG_NO_TIMESTAMPS
/**
 * Calibrate rdtsc() frequency by measuring against wall-clock time.
 * This is called once at initialization to determine CPU frequency.
 */
static void calibrate_timestamp(void) {
    struct timespec ts1, ts2;
    uint64_t ticks1, ticks2;

    /* Measure CPU frequency over 100ms */
    clock_gettime(CLOCK_REALTIME, &ts1);
    ticks1 = rdtsc();

    /* Sleep for 100 milliseconds */
    struct timespec sleep_time = {0, 100000000};  /* 100ms = 0.1s */
    nanosleep(&sleep_time, NULL);

    clock_gettime(CLOCK_REALTIME, &ts2);
    ticks2 = rdtsc();

    /* Calculate elapsed time in seconds */
    double elapsed_sec = (ts2.tv_sec - ts1.tv_sec) +
                         (ts2.tv_nsec - ts1.tv_nsec) / 1e9;

    /* Calculate ticks per second (CPU frequency) */
    g_timestamp_frequency = (uint64_t)((ticks2 - ticks1) / elapsed_sec);

    /* Record start time and timestamp */
    g_start_time_sec = ts1.tv_sec;
    g_start_time_nsec = ts1.tv_nsec;
    g_start_timestamp = ticks1;
}
#endif

/* ============================================================================
 * Initialization
 * ============================================================================ */

int cnanolog_init(const char* log_file_path) {
    if (g_is_initialized) {
        return 0;  /* Already initialized */
    }

    /* Initialize registry */
    log_registry_init(&g_registry);

    /* Create binary writer */
    g_binary_writer = binwriter_create(log_file_path);
    if (g_binary_writer == NULL) {
        fprintf(stderr, "cnanolog_init: Failed to create binary writer\n");
        log_registry_destroy(&g_registry);
        return -1;
    }

    /* Calibrate timestamp (Phase 5: Measure CPU frequency) */
#ifndef CNANOLOG_NO_TIMESTAMPS
    calibrate_timestamp();

    /* Write file header with calibrated frequency */
    if (binwriter_write_header(g_binary_writer,
                               g_timestamp_frequency,  /* Calibrated CPU frequency */
                               g_start_timestamp,
                               g_start_time_sec,
                               g_start_time_nsec) != 0) {
#else
    /* No timestamps - write header with zeros for timestamp fields */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    if (binwriter_write_header(g_binary_writer,
                               0,  /* No timestamp frequency */
                               0,  /* No start timestamp */
                               ts.tv_sec,
                               (int32_t)ts.tv_nsec) != 0) {
#endif
        fprintf(stderr, "cnanolog_init: Failed to write header\n");
        binwriter_close(g_binary_writer, NULL, 0);
        log_registry_destroy(&g_registry);
        return -1;
    }

    /* Initialize buffer registry */
    buffer_registry_init(&g_buffer_registry);

    /* Start background writer thread */
    if (cnanolog_thread_create(&g_writer_thread, writer_thread_main, NULL) != 0) {
        fprintf(stderr, "cnanolog_init: Failed to create writer thread\n");
        binwriter_close(g_binary_writer, NULL, 0);
        log_registry_destroy(&g_registry);
        return -1;
    }

    g_is_initialized = 1;
    return 0;
}

/* ============================================================================
 * Shutdown
 * ============================================================================ */

void cnanolog_shutdown(void) {
    if (!g_is_initialized) {
        return;
    }

    /* Signal writer thread to exit */
    g_should_exit = 1;
    cnanolog_thread_join(g_writer_thread, NULL);

    /*
     * Final flush of all staging buffers.
     * Safe without lock: background thread has exited, no new logs can arrive.
     */
    char temp_buf[MAX_LOG_ENTRY_SIZE];
    uint32_t num_buffers = g_buffer_registry.count;  /* Read atomic counter */

    for (size_t i = 0; i < num_buffers; i++) {
        /* Use atomic load with acquire semantics to see all previous writes */
#if defined(__GNUC__) || defined(__clang__)
        staging_buffer_t* sb = __atomic_load_n(&g_buffer_registry.buffers[i], __ATOMIC_ACQUIRE);
#else
        staging_buffer_t* sb = g_buffer_registry.buffers[i];
#endif
        if (sb == NULL) continue;

        /* Drain remaining data from this buffer */
        while (staging_available(sb) > 0) {
            size_t nread = staging_read(sb, temp_buf, MAX_LOG_ENTRY_SIZE);
            if (nread == 0) break;

            /* Parse entry header */
            cnanolog_entry_header_t* header = (cnanolog_entry_header_t*)temp_buf;

            /* Check for wrap marker (circular buffer wrap-around) */
            if (header->log_id == STAGING_WRAP_MARKER_LOG_ID) {
                /* This is a wrap marker - skip it and wrap read position */
                staging_consume(sb, sizeof(cnanolog_entry_header_t));
                staging_wrap_read_pos(sb);
                continue;  /* Continue processing from beginning */
            }

            /* Write real log entry */
            binwriter_write_entry(g_binary_writer,
                                header->log_id,
#ifndef CNANOLOG_NO_TIMESTAMPS
                                header->timestamp,
#else
                                0,  /* No timestamp */
#endif
                                temp_buf + sizeof(cnanolog_entry_header_t),
                                header->data_length);

            staging_consume(sb, nread);
        }

        /* Destroy the buffer */
        staging_buffer_destroy(sb);
    }
    g_buffer_registry.count = 0;

    /* Get all registered sites for dictionary */
    uint32_t num_sites = 0;
    const log_site_t* sites = log_registry_get_all(&g_registry, &num_sites);

    /* Close binary writer (writes dictionary) */
    if (binwriter_close(g_binary_writer, sites, num_sites) != 0) {
        fprintf(stderr, "cnanolog_shutdown: Failed to close binary writer\n");
    }

    /* Clean up */
    log_registry_destroy(&g_registry);

    g_is_initialized = 0;
}

/* ============================================================================
 * Log Site Registration
 * ============================================================================ */

uint32_t _cnanolog_register_site(cnanolog_level_t level,
                                  const char* filename,
                                  uint32_t line_number,
                                  const char* format,
                                  uint8_t num_args,
                                  const uint8_t* arg_types) {
    if (!g_is_initialized) {
        return UINT32_MAX;
    }

    return log_registry_register(&g_registry, level, filename, line_number,
                                format, num_args, arg_types);
}

/* ============================================================================
 * Binary Logging
 * ============================================================================ */

void _cnanolog_log_binary(uint32_t log_id,
                          uint8_t num_args,
                          const uint8_t* arg_types,
                          ...) {
    /* Fast path: almost always initialized and log_id is valid */
    if (unlikely(!g_is_initialized || log_id == UINT32_MAX)) {
        return;
    }

    /* Track statistics: log written */
#if !defined(CNANOLOG_NO_TIMESTAMPS) && !defined(CNANOLOG_NO_STATISTICS)
    g_stats.total_logs++;
#endif

    /* Get thread-local staging buffer (lazy initialization, no lock!) */
    staging_buffer_t* sb = get_or_create_staging_buffer();
    if (unlikely(sb == NULL)) {
#if !defined(CNANOLOG_NO_TIMESTAMPS) && !defined(CNANOLOG_NO_STATISTICS)
        g_stats.dropped_logs++;
#endif
        return;  /* Failed to allocate buffer */
    }

    size_t max_entry_size = MAX_LOG_ENTRY_SIZE;
    char* write_ptr = staging_reserve(sb, max_entry_size);
    if (unlikely(write_ptr == NULL)) {
        /* Buffer full, drop log */
#if !defined(CNANOLOG_NO_TIMESTAMPS) && !defined(CNANOLOG_NO_STATISTICS)
        g_stats.dropped_logs++;
#endif
        return;
    }

    /* Write entry header (we'll update data_length after packing) */
    cnanolog_entry_header_t* header = (cnanolog_entry_header_t*)write_ptr;
    header->log_id = log_id;
#ifndef CNANOLOG_NO_TIMESTAMPS
    header->timestamp = get_timestamp();
#endif

    /* Pack arguments in SINGLE PASS (calculates size while packing) */
    size_t arg_data_size = 0;
    if (num_args > 0) {
        va_list args;
        va_start(args, arg_types);
        char* arg_data = write_ptr + sizeof(cnanolog_entry_header_t);
        arg_data_size = arg_pack_write_fast(arg_data,
                                             max_entry_size - sizeof(cnanolog_entry_header_t),
                                             num_args, arg_types, args);
        va_end(args);

        if (unlikely(arg_data_size == 0)) {
            /* Packing failed (buffer too small or error) - give back all reserved space */
            staging_adjust_reservation(sb, max_entry_size, 0);
#if !defined(CNANOLOG_NO_TIMESTAMPS) && !defined(CNANOLOG_NO_STATISTICS)
            g_stats.dropped_logs++;
#endif
            return;
        }
    }
    /* If num_args == 0, arg_data_size stays 0 (valid case) */

    /* Update header with actual size */
    header->data_length = (uint16_t)arg_data_size;
    size_t actual_entry_size = sizeof(cnanolog_entry_header_t) + arg_data_size;

    /* Adjust reservation to actual size and commit */
    staging_adjust_reservation(sb, max_entry_size, actual_entry_size);
    staging_commit(sb, actual_entry_size);

    /* No need to signal - background thread polls all buffers */
}

/* ============================================================================
 * Background Writer Thread
 * ============================================================================ */

static void* writer_thread_main(void* arg) {
    (void)arg;
    char temp_buf[MAX_LOG_ENTRY_SIZE];
    char compressed_buf[MAX_LOG_ENTRY_SIZE];
    size_t last_checked_idx = 0;

    /* Batch processing state */
    size_t entries_since_flush = 0;
#ifndef CNANOLOG_NO_TIMESTAMPS
    uint64_t last_flush_time = get_timestamp();
#endif

    while (!g_should_exit) {
        int found_work = 0;

        /* Track background thread activity */
#if !defined(CNANOLOG_NO_TIMESTAMPS) && !defined(CNANOLOG_NO_STATISTICS)
        g_stats.background_wakeups++;
#endif

        /* Get current number of buffers (lock-free read) */
        size_t num_buffers = g_buffer_registry.count;

        /* Round-robin through all staging buffers */
        for (size_t i = 0; i < num_buffers; i++) {
            size_t idx = (last_checked_idx + i) % num_buffers;

            /* Get buffer with atomic load (acquire semantics for visibility) */
#if defined(__GNUC__) || defined(__clang__)
            staging_buffer_t* sb = __atomic_load_n(&g_buffer_registry.buffers[idx], __ATOMIC_ACQUIRE);
#else
            staging_buffer_t* sb = g_buffer_registry.buffers[idx];
#endif
            if (sb == NULL) {
                continue;
            }

            /* Check if data is available */
            size_t available = staging_available(sb);
            if (available == 0) {
                continue;  /* Buffer is empty, try next */
            }

            /* Process entries from this buffer one at a time */
            while (staging_available(sb) >= sizeof(cnanolog_entry_header_t)) {
                /* Peek at header to determine entry size */
                size_t nread = staging_read(sb, temp_buf, sizeof(cnanolog_entry_header_t));
                if (nread < sizeof(cnanolog_entry_header_t)) {
                    break;  /* Not enough data for header */
                }

                /* Parse entry header to get total size */
                cnanolog_entry_header_t* header = (cnanolog_entry_header_t*)temp_buf;

                /* Check for wrap marker (circular buffer wrap-around) */
                if (header->log_id == STAGING_WRAP_MARKER_LOG_ID) {
                    /* This is a wrap marker - consumer should wrap to beginning */
                    staging_consume(sb, sizeof(cnanolog_entry_header_t));
                    staging_wrap_read_pos(sb);
                    continue;  /* Continue processing from beginning */
                }

                size_t entry_size = sizeof(cnanolog_entry_header_t) + header->data_length;

                /* Check if we have the full entry available */
                if (staging_available(sb) < entry_size) {
                    break;  /* Don't have complete entry yet, wait for more data */
                }

                /* Now read the complete entry (staging_read is a peek, so we read from start again) */
                nread = staging_read(sb, temp_buf, entry_size);
                if (nread < entry_size) {
                    break;  /* Couldn't read full entry */
                }

                /* Reparse header from full entry */
                header = (cnanolog_entry_header_t*)temp_buf;

                /* Get log site info for compression */
                const log_site_t* site = log_registry_get(&g_registry, header->log_id);

                /* Compress argument data */
                size_t compressed_len = 0;
                const char* data_to_write;
                uint16_t data_len_to_write;

                if (site != NULL && site->num_args > 0) {
                    /* Compress the argument data */
                    int compress_result = compress_entry_args(
                        temp_buf + sizeof(cnanolog_entry_header_t),
                        header->data_length,
                        compressed_buf,
                        &compressed_len,
                        site);

                    if (compress_result == 0) {
                        /* Compression succeeded */
                        data_to_write = compressed_buf;
                        data_len_to_write = (uint16_t)compressed_len;

                        /* Track compression statistics */
#if !defined(CNANOLOG_NO_TIMESTAMPS) && !defined(CNANOLOG_NO_STATISTICS)
                        g_stats.bytes_compressed_from += header->data_length;
                        g_stats.bytes_compressed_to += compressed_len;
#endif
                    } else {
                        /* Compression failed, write uncompressed */
                        data_to_write = temp_buf + sizeof(cnanolog_entry_header_t);
                        data_len_to_write = header->data_length;
                    }
                } else {
                    /* No arguments or invalid site, write uncompressed */
                    data_to_write = temp_buf + sizeof(cnanolog_entry_header_t);
                    data_len_to_write = header->data_length;
                }

                /* Write to binary file (compressed or uncompressed) */
                binwriter_write_entry(g_binary_writer,
                                    header->log_id,
#ifndef CNANOLOG_NO_TIMESTAMPS
                                    header->timestamp,
#else
                                    0,  /* No timestamp */
#endif
                                    data_to_write,
                                    data_len_to_write);

                /* Mark this entry as consumed */
                staging_consume(sb, entry_size);

                entries_since_flush++;
                found_work = 1;
            }
        }

        /* Update round-robin position */
        if (num_buffers > 0) {
            last_checked_idx = (last_checked_idx + 1) % num_buffers;
        }

        /*
         * Batch flush strategy:
         * Flush when we've written N entries OR when enough time has passed (if timestamps enabled).
         * This reduces flush frequency while ensuring data doesn't sit too long.
         */
#ifndef CNANOLOG_NO_TIMESTAMPS
        uint64_t now = get_timestamp();
        uint64_t elapsed_ns = now - last_flush_time;
        uint64_t elapsed_ms = elapsed_ns / 1000000;

        if (entries_since_flush >= FLUSH_BATCH_SIZE ||
            elapsed_ms >= FLUSH_INTERVAL_MS ||
            (entries_since_flush > 0 && !found_work)) {
#else
        /* Without timestamps, flush based on entry count only */
        if (entries_since_flush >= FLUSH_BATCH_SIZE ||
            (entries_since_flush > 0 && !found_work)) {
#endif
            /* Flush to disk */
            binwriter_flush(g_binary_writer);

            entries_since_flush = 0;
#ifndef CNANOLOG_NO_TIMESTAMPS
            last_flush_time = now;
#endif
        }

        /* If no work was found, sleep briefly to avoid spinning */
        if (!found_work) {
            /* Sleep for 100 microseconds */
            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = 100000;  /* 100us */
            nanosleep(&ts, NULL);
        }
    }

    return NULL;
}

/* ============================================================================
 * Buffer Registry Implementation
 * ============================================================================ */

/**
 * Initialize lock-free buffer registry.
 * No mutex needed - uses atomic operations for thread safety.
 */
static void buffer_registry_init(buffer_registry_t* registry) {
    memset(registry->buffers, 0, sizeof(registry->buffers));
    registry->count = 0;
}

/**
 * Lock-free buffer registry add operation.
 *
 * Uses atomic fetch-and-add to claim a unique slot, then stores
 * the buffer pointer with release semantics for visibility.
 *
 * Thread-safe: Multiple threads can register simultaneously
 */
static int buffer_registry_add(buffer_registry_t* registry, staging_buffer_t* buffer) {
    /* Atomically claim a slot - this is lock-free! */
#if defined(__GNUC__) || defined(__clang__)
    uint32_t idx = __atomic_fetch_add(&registry->count, 1, __ATOMIC_SEQ_CST);
#else
    /* Fallback for compilers without atomics (not thread-safe) */
    uint32_t idx = registry->count++;
#endif

    /* Check if we exceeded capacity */
    if (unlikely(idx >= MAX_STAGING_BUFFERS)) {
        fprintf(stderr, "cnanolog: Buffer registry full (max %d threads)\n", MAX_STAGING_BUFFERS);
        return -1;
    }

    /* Store buffer pointer with release semantics */
#if defined(__GNUC__) || defined(__clang__)
    __atomic_store_n(&registry->buffers[idx], buffer, __ATOMIC_RELEASE);
#else
    registry->buffers[idx] = buffer;
#endif

    return 0;
}

/* ============================================================================
 * Thread-Local Buffer Management
 * ============================================================================ */

/**
 * Get or create the thread-local staging buffer.
 * This is called on the first log from each thread.
 * Subsequent calls return the cached pointer (very fast).
 */
static staging_buffer_t* get_or_create_staging_buffer(void) {
    /* Fast path: buffer already exists */
    if (tls_staging_buffer != NULL) {
        return tls_staging_buffer;
    }

    /* Slow path: allocate new buffer (first log from this thread) */

    /* Generate thread ID */
#if defined(__GNUC__) || defined(__clang__)
    uint32_t thread_id = __atomic_fetch_add(&g_next_thread_id, 1, __ATOMIC_SEQ_CST);
#else
    /* Fallback: not thread-safe but better than nothing */
    uint32_t thread_id = g_next_thread_id++;
#endif

    /* Create staging buffer */
    staging_buffer_t* sb = staging_buffer_create(thread_id);
    if (sb == NULL) {
        fprintf(stderr, "cnanolog: Failed to allocate staging buffer for thread %u\n", thread_id);
        return NULL;
    }

    /* Register with global registry */
    if (buffer_registry_add(&g_buffer_registry, sb) != 0) {
        fprintf(stderr, "cnanolog: Failed to register staging buffer (max threads exceeded)\n");
        staging_buffer_destroy(sb);
        return NULL;
    }

    /* Cache in thread-local storage */
    tls_staging_buffer = sb;

    return sb;
}

/* ============================================================================
 * Timestamp
 * ============================================================================ */

#ifndef CNANOLOG_NO_TIMESTAMPS
/**
 * Get current timestamp using rdtsc (Phase 5: High-resolution timestamps).
 */
static uint64_t get_timestamp(void) {
    return rdtsc();  /* ~5-10ns overhead */
}
#endif

/* ============================================================================
 * Statistics API Implementation
 * ============================================================================ */

void cnanolog_get_stats(cnanolog_stats_t* stats) {
    if (stats == NULL) {
        return;
    }

#if !defined(CNANOLOG_NO_TIMESTAMPS) && !defined(CNANOLOG_NO_STATISTICS)
    /* Read counters (volatile reads for thread safety) */
    stats->total_logs_written = g_stats.total_logs;
    stats->dropped_logs = g_stats.dropped_logs;
    stats->background_wakeups = g_stats.background_wakeups;

    /* Get bytes written from binary writer */
    if (g_binary_writer != NULL) {
        stats->total_bytes_written = binwriter_get_bytes_written(g_binary_writer);
    } else {
        stats->total_bytes_written = 0;
    }

    /* Calculate compression ratio */
    if (g_stats.bytes_compressed_from > 0) {
        stats->compression_ratio_x100 =
            (g_stats.bytes_compressed_from * 100) / g_stats.bytes_compressed_to;
    } else {
        stats->compression_ratio_x100 = 100;  /* 1.00x (no compression yet) */
    }

    /* Count active staging buffers */
    stats->staging_buffers_active = g_buffer_registry.count;
#else
    /* Statistics disabled in extreme performance mode */
    memset(stats, 0, sizeof(*stats));
#endif
}

void cnanolog_reset_stats(void) {
#if !defined(CNANOLOG_NO_TIMESTAMPS) && !defined(CNANOLOG_NO_STATISTICS)
    /* Reset all counters to zero */
    g_stats.total_logs = 0;
    g_stats.dropped_logs = 0;
    g_stats.bytes_written = 0;
    g_stats.bytes_compressed_from = 0;
    g_stats.bytes_compressed_to = 0;
    g_stats.background_wakeups = 0;
#endif
}

void cnanolog_preallocate(void) {
    /* Force allocation of thread-local buffer */
    staging_buffer_t* sb = get_or_create_staging_buffer();
    (void)sb;  /* Suppress unused warning */
}

/**
 * Set CPU affinity for the background writer thread.
 * See cnanolog.h for full documentation.
 */
int cnanolog_set_writer_affinity(int core_id) {
    if (!g_is_initialized) {
        fprintf(stderr, "cnanolog_set_writer_affinity: Logger not initialized\n");
        return -1;
    }

    if (core_id < 0) {
        fprintf(stderr, "cnanolog_set_writer_affinity: Invalid core ID %d\n", core_id);
        return -1;
    }

    /* Call platform-specific affinity function */
    int result = cnanolog_thread_set_affinity(g_writer_thread, core_id);

    if (result == 0) {
        /* Success - affinity set */
        return 0;
    } else {
        /* Failed - warning already printed by platform layer */
        return -1;
    }
}
