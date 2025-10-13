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

/* Batch processing configuration */
#define FLUSH_BATCH_SIZE 100           /* Flush every N entries */
#define FLUSH_INTERVAL_MS 100          /* OR flush every N milliseconds */

/* ============================================================================
 * Global State
 * ============================================================================ */

static log_registry_t g_registry;
static binary_writer_t* g_binary_writer = NULL;
static volatile int g_should_exit = 0;
static int g_is_initialized = 0;

/* Background writer thread */
static thread_t g_writer_thread;

/* Timing for binary logs */
static uint64_t g_start_timestamp = 0;
static time_t g_start_time_sec = 0;
static int32_t g_start_time_nsec = 0;
static uint64_t g_timestamp_frequency = 0;  /* CPU frequency (Hz) for rdtsc() */

/* ============================================================================
 * Global Statistics Tracking
 * ============================================================================ */

static struct {
    volatile uint64_t total_logs;            /* Logs written by threads */
    volatile uint64_t dropped_logs;          /* Logs dropped (buffer full) */
    volatile uint64_t bytes_written;         /* Bytes written to file */
    volatile uint64_t bytes_compressed_from; /* Uncompressed size */
    volatile uint64_t bytes_compressed_to;   /* Compressed size */
    volatile uint64_t background_wakeups;    /* Background thread wakeups */
} g_stats = {0, 0, 0, 0, 0, 0};

/* ============================================================================
 * Global Buffer Registry (for background thread to find all buffers)
 * ============================================================================ */

typedef struct {
    staging_buffer_t* buffers[MAX_STAGING_BUFFERS];
    size_t count;
    mutex_t lock;  /* Only used during add (rare operation) */
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
    calibrate_timestamp();

    /* Write file header with calibrated frequency */
    if (binwriter_write_header(g_binary_writer,
                               g_timestamp_frequency,  /* Calibrated CPU frequency */
                               g_start_timestamp,
                               g_start_time_sec,
                               g_start_time_nsec) != 0) {
        fprintf(stderr, "cnanolog_init: Failed to write header\n");
        binwriter_close(g_binary_writer, NULL, 0);
        log_registry_destroy(&g_registry);
        return -1;
    }

    /* Initialize buffer registry */
    buffer_registry_init(&g_buffer_registry);

    /* Start background writer thread */
    if (thread_create(&g_writer_thread, writer_thread_main, NULL) != 0) {
        fprintf(stderr, "cnanolog_init: Failed to create writer thread\n");
        binwriter_close(g_binary_writer, NULL, 0);
        log_registry_destroy(&g_registry);
        mutex_destroy(&g_buffer_registry.lock);
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
    thread_join(g_writer_thread, NULL);

    /* Final flush of all staging buffers */
    char temp_buf[MAX_LOG_ENTRY_SIZE];
    mutex_lock(&g_buffer_registry.lock);
    for (size_t i = 0; i < g_buffer_registry.count; i++) {
        staging_buffer_t* sb = g_buffer_registry.buffers[i];

        /* Drain remaining data from this buffer */
        while (staging_available(sb) > 0) {
            size_t nread = staging_read(sb, temp_buf, MAX_LOG_ENTRY_SIZE);
            if (nread == 0) break;

            /* Parse and write entry */
            cnanolog_entry_header_t* header = (cnanolog_entry_header_t*)temp_buf;
            binwriter_write_entry(g_binary_writer,
                                header->log_id,
                                header->timestamp,
                                temp_buf + sizeof(cnanolog_entry_header_t),
                                header->data_length);

            staging_consume(sb, nread);
        }

        /* Destroy the buffer */
        staging_buffer_destroy(sb);
    }
    g_buffer_registry.count = 0;
    mutex_unlock(&g_buffer_registry.lock);

    /* Get all registered sites for dictionary */
    uint32_t num_sites = 0;
    const log_site_t* sites = log_registry_get_all(&g_registry, &num_sites);

    /* Close binary writer (writes dictionary) */
    if (binwriter_close(g_binary_writer, sites, num_sites) != 0) {
        fprintf(stderr, "cnanolog_shutdown: Failed to close binary writer\n");
    }

    /* Clean up */
    log_registry_destroy(&g_registry);
    mutex_destroy(&g_buffer_registry.lock);

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
    if (!g_is_initialized || log_id == UINT32_MAX) {
        return;
    }

    /* Track statistics: log written */
    g_stats.total_logs++;

    /* Get thread-local staging buffer (lazy initialization, no lock!) */
    staging_buffer_t* sb = get_or_create_staging_buffer();
    if (sb == NULL) {
        g_stats.dropped_logs++;
        return;  /* Failed to allocate buffer */
    }

    /* Calculate argument data size */
    va_list args, args_copy;
    va_start(args, arg_types);
    va_copy(args_copy, args);
    size_t arg_data_size = arg_pack_calc_size(num_args, arg_types, args_copy);
    va_end(args_copy);

    size_t entry_size = sizeof(cnanolog_entry_header_t) + arg_data_size;

    if (entry_size > MAX_LOG_ENTRY_SIZE) {
        /* Entry too large, drop it */
        va_end(args);
        return;
    }

    /*
     * LOCK-FREE FAST PATH:
     * Reserve space in thread-local buffer (just pointer arithmetic)
     */
    char* write_ptr = staging_reserve(sb, entry_size);
    if (write_ptr == NULL) {
        /* Buffer full, drop log */
        g_stats.dropped_logs++;
        va_end(args);
        return;
    }

    /* Write entry header */
    cnanolog_entry_header_t* header = (cnanolog_entry_header_t*)write_ptr;
    header->log_id = log_id;
    header->timestamp = get_timestamp();
    header->data_length = (uint16_t)arg_data_size;

    /* Pack arguments directly into reserved space */
    char* arg_data = write_ptr + sizeof(cnanolog_entry_header_t);
    arg_pack_write(arg_data, arg_data_size, num_args, arg_types, args);
    va_end(args);

    /*
     * Commit the write (atomic with memory fence)
     * This makes the data visible to the background thread
     */
    staging_commit(sb, entry_size);

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
    uint64_t last_flush_time = get_timestamp();

    while (!g_should_exit) {
        int found_work = 0;

        /* Track background thread activity */
        g_stats.background_wakeups++;

        /* Get current number of buffers (lock-free read) */
        size_t num_buffers = g_buffer_registry.count;

        /* Round-robin through all staging buffers */
        for (size_t i = 0; i < num_buffers; i++) {
            size_t idx = (last_checked_idx + i) % num_buffers;

            /* Get buffer (no lock needed - array is append-only during runtime) */
            staging_buffer_t* sb = g_buffer_registry.buffers[idx];
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
                        g_stats.bytes_compressed_from += header->data_length;
                        g_stats.bytes_compressed_to += compressed_len;
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
                                    header->timestamp,
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
         * Flush when we've written N entries OR when enough time has passed.
         * This reduces flush frequency while ensuring data doesn't sit too long.
         */
        uint64_t now = get_timestamp();
        uint64_t elapsed_ns = now - last_flush_time;
        uint64_t elapsed_ms = elapsed_ns / 1000000;

        if (entries_since_flush >= FLUSH_BATCH_SIZE ||
            elapsed_ms >= FLUSH_INTERVAL_MS ||
            (entries_since_flush > 0 && !found_work)) {
            /* Flush to disk */
            binwriter_flush(g_binary_writer);

            entries_since_flush = 0;
            last_flush_time = now;
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

static void buffer_registry_init(buffer_registry_t* registry) {
    memset(registry->buffers, 0, sizeof(registry->buffers));
    registry->count = 0;
    mutex_init(&registry->lock);
}

static int buffer_registry_add(buffer_registry_t* registry, staging_buffer_t* buffer) {
    mutex_lock(&registry->lock);

    if (registry->count >= MAX_STAGING_BUFFERS) {
        mutex_unlock(&registry->lock);
        return -1;  /* Registry full */
    }

    /* Add buffer to registry */
    registry->buffers[registry->count] = buffer;
    registry->count++;

    mutex_unlock(&registry->lock);
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

/**
 * Get current timestamp (simple counter for now, will use rdtsc in Phase 5).
 */
static uint64_t get_timestamp(void) {
    return rdtsc();  /* Phase 5: High-resolution timestamps (~5-10ns overhead) */
}

/* ============================================================================
 * Statistics API Implementation
 * ============================================================================ */

void cnanolog_get_stats(cnanolog_stats_t* stats) {
    if (stats == NULL) {
        return;
    }

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
}

void cnanolog_reset_stats(void) {
    /* Reset all counters to zero */
    g_stats.total_logs = 0;
    g_stats.dropped_logs = 0;
    g_stats.bytes_written = 0;
    g_stats.bytes_compressed_from = 0;
    g_stats.bytes_compressed_to = 0;
    g_stats.background_wakeups = 0;
}

void cnanolog_preallocate(void) {
    /* Force allocation of thread-local buffer */
    staging_buffer_t* sb = get_or_create_staging_buffer();
    (void)sb;  /* Suppress unused warning */
}
