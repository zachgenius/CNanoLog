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
 * Statistics Tracking
 * ============================================================================ */

/* Global statistics (updated with atomic operations) */
static struct {
    volatile uint64_t total_logs_attempted;
    volatile uint64_t total_logs_dropped;
    volatile uint64_t total_bytes_written;
    volatile uint64_t background_flushes;
} g_stats;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void* writer_thread_main(void* arg);
static uint64_t get_timestamp(void);
static void buffer_registry_init(buffer_registry_t* registry);
static int buffer_registry_add(buffer_registry_t* registry, staging_buffer_t* buffer);
static staging_buffer_t* get_or_create_staging_buffer(void);

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

    /* Record start time */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    g_start_time_sec = ts.tv_sec;
    g_start_time_nsec = (int32_t)ts.tv_nsec;
    g_start_timestamp = get_timestamp();

    /* Write file header (use simple counter for now, rdtsc in Phase 5) */
    if (binwriter_write_header(g_binary_writer,
                               1000000000ULL,  /* 1 GHz frequency (placeholder) */
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

    /* Convert to log_site_info_t format for binary_writer */
    log_site_info_t* site_infos = NULL;
    if (num_sites > 0) {
        site_infos = (log_site_info_t*)malloc(num_sites * sizeof(log_site_info_t));
        for (uint32_t i = 0; i < num_sites; i++) {
            site_infos[i].log_id = sites[i].log_id;
            site_infos[i].log_level = sites[i].log_level;
            site_infos[i].filename = sites[i].filename;
            site_infos[i].format = sites[i].format;
            site_infos[i].line_number = sites[i].line_number;
            site_infos[i].num_args = sites[i].num_args;
            memcpy(site_infos[i].arg_types, sites[i].arg_types, CNANOLOG_MAX_ARGS);
        }
    }

    /* Close binary writer (writes dictionary) */
    if (binwriter_close(g_binary_writer, site_infos, num_sites) != 0) {
        fprintf(stderr, "cnanolog_shutdown: Failed to close binary writer\n");
    }

    if (site_infos != NULL) {
        free(site_infos);
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

    /* Track statistics: log attempt */
#if defined(__GNUC__) || defined(__clang__)
    __atomic_fetch_add(&g_stats.total_logs_attempted, 1, __ATOMIC_RELAXED);
#else
    g_stats.total_logs_attempted++;
#endif

    /* Get thread-local staging buffer (lazy initialization, no lock!) */
    staging_buffer_t* sb = get_or_create_staging_buffer();
    if (sb == NULL) {
#if defined(__GNUC__) || defined(__clang__)
        __atomic_fetch_add(&g_stats.total_logs_dropped, 1, __ATOMIC_RELAXED);
#else
        g_stats.total_logs_dropped++;
#endif
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
#if defined(__GNUC__) || defined(__clang__)
        __atomic_fetch_add(&g_stats.total_logs_dropped, 1, __ATOMIC_RELAXED);
#else
        g_stats.total_logs_dropped++;
#endif
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

                /* Track statistics: bytes written (compressed size) */
#if defined(__GNUC__) || defined(__clang__)
                __atomic_fetch_add(&g_stats.total_bytes_written,
                                  sizeof(cnanolog_entry_header_t) + data_len_to_write,
                                  __ATOMIC_RELAXED);
#else
                g_stats.total_bytes_written += sizeof(cnanolog_entry_header_t) + data_len_to_write;
#endif

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

            /* Track statistics: flush */
#if defined(__GNUC__) || defined(__clang__)
            __atomic_fetch_add(&g_stats.background_flushes, 1, __ATOMIC_RELAXED);
#else
            g_stats.background_flushes++;
#endif

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
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* ============================================================================
 * Statistics API Implementation
 * ============================================================================ */

void cnanolog_get_stats(cnanolog_stats_t* stats) {
    if (stats == NULL) {
        return;
    }

    /* Read counters (atomic for thread safety) */
#if defined(__GNUC__) || defined(__clang__)
    stats->total_logs_attempted = __atomic_load_n(&g_stats.total_logs_attempted, __ATOMIC_RELAXED);
    stats->total_logs_dropped = __atomic_load_n(&g_stats.total_logs_dropped, __ATOMIC_RELAXED);
    stats->total_bytes_written = __atomic_load_n(&g_stats.total_bytes_written, __ATOMIC_RELAXED);
    stats->background_flushes = __atomic_load_n(&g_stats.background_flushes, __ATOMIC_RELAXED);
#else
    stats->total_logs_attempted = g_stats.total_logs_attempted;
    stats->total_logs_dropped = g_stats.total_logs_dropped;
    stats->total_bytes_written = g_stats.total_bytes_written;
    stats->background_flushes = g_stats.background_flushes;
#endif

    /* Calculate derived stats */
    stats->total_logs_written = stats->total_logs_attempted - stats->total_logs_dropped;

    /* Current state */
    stats->active_threads = 0;
    stats->total_buffers = (uint32_t)g_buffer_registry.count;
    stats->max_buffer_fill_percent = 0;

    /* Scan all buffers to get active count and max fill */
    for (size_t i = 0; i < g_buffer_registry.count; i++) {
        staging_buffer_t* sb = g_buffer_registry.buffers[i];
        if (sb != NULL) {
            if (sb->active) {
                stats->active_threads++;
            }

            uint8_t fill = staging_fill_percent(sb);
            if (fill > stats->max_buffer_fill_percent) {
                stats->max_buffer_fill_percent = fill;
            }
        }
    }
}

void cnanolog_reset_stats(void) {
    /* Reset all counters to zero (atomic) */
#if defined(__GNUC__) || defined(__clang__)
    __atomic_store_n(&g_stats.total_logs_attempted, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_stats.total_logs_dropped, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_stats.total_bytes_written, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_stats.background_flushes, 0, __ATOMIC_RELAXED);
#else
    g_stats.total_logs_attempted = 0;
    g_stats.total_logs_dropped = 0;
    g_stats.total_bytes_written = 0;
    g_stats.background_flushes = 0;
#endif
}
