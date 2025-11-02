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
 * Flushes when: FLUSH_BATCH_SIZE entries OR FLUSH_INTERVAL_MS elapsed OR buffer empty.
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

/* Rotation state */
static cnanolog_rotation_policy_t g_rotation_policy = CNANOLOG_ROTATE_NONE;
static char g_base_path[512] = {0};  /* Base path for rotated files */
static int g_current_day = -1;       /* Current day of year (for rotation check) */

/* Custom level registry */
typedef struct {
    uint8_t level;
    char name[32];
} custom_level_t;

static custom_level_t g_custom_levels[CNANOLOG_MAX_CUSTOM_LEVELS];
static volatile uint32_t g_custom_level_count = 0;

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
static void generate_dated_filename(const char* base_path, char* output, size_t output_size);
static int check_and_rotate_if_needed(void);

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
 * Custom Level Registration
 * ============================================================================ */

int cnanolog_register_level(const char* name, uint8_t level) {
    if (name == NULL) {
        fprintf(stderr, "cnanolog_register_level: name is NULL\n");
        return -1;
    }

    if (g_is_initialized) {
        fprintf(stderr, "cnanolog_register_level: Cannot register levels after init\n");
        return -1;
    }

    if (level < 4) {
        fprintf(stderr, "cnanolog_register_level: Level %u is reserved (0-3)\n", level);
        return -1;
    }

    if (g_custom_level_count >= CNANOLOG_MAX_CUSTOM_LEVELS) {
        fprintf(stderr, "cnanolog_register_level: Maximum custom levels reached (%d)\n",
                CNANOLOG_MAX_CUSTOM_LEVELS);
        return -1;
    }

    /* Check for duplicate level value */
    for (uint32_t i = 0; i < g_custom_level_count; i++) {
        if (g_custom_levels[i].level == level) {
            fprintf(stderr, "cnanolog_register_level: Level %u already registered as '%s'\n",
                    level, g_custom_levels[i].name);
            return -1;
        }
    }

    /* Add to registry */
    g_custom_levels[g_custom_level_count].level = level;
    strncpy(g_custom_levels[g_custom_level_count].name, name, sizeof(g_custom_levels[0].name) - 1);
    g_custom_levels[g_custom_level_count].name[sizeof(g_custom_levels[0].name) - 1] = '\0';
    g_custom_level_count++;

    return 0;
}

/**
 * Get all registered custom levels (for writing to binary file).
 * Internal function used by binary_writer.
 */
const custom_level_t* _cnanolog_get_custom_levels(uint32_t* count) {
    if (count != NULL) {
        *count = g_custom_level_count;
    }
    return g_custom_levels;
}

/* ============================================================================
 * Rotation Helpers
 * ============================================================================ */

/**
 * Generate a dated filename from a base path.
 * Example: "logs/app.clog" -> "logs/app-2025-11-02.clog"
 */
static void generate_dated_filename(const char* base_path, char* output, size_t output_size) {
    /* Get current date */
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);

    /* Find the extension */
    const char* ext = strrchr(base_path, '.');
    if (ext == NULL) {
        /* No extension, append date at end */
        snprintf(output, output_size, "%s-%04d-%02d-%02d",
                base_path,
                tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
    } else {
        /* Insert date before extension */
        size_t base_len = ext - base_path;
        snprintf(output, output_size, "%.*s-%04d-%02d-%02d%s",
                (int)base_len, base_path,
                tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                ext);
    }
}

/**
 * Check if date has changed and rotate log file if needed.
 * Returns 0 on success, -1 on error.
 */
static int check_and_rotate_if_needed(void) {
    if (g_rotation_policy != CNANOLOG_ROTATE_DAILY) {
        return 0;  /* Rotation not enabled */
    }

    /* Get current day */
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);
    int day_of_year = tm->tm_yday;

    /* Initialize current day on first check */
    if (g_current_day == -1) {
        g_current_day = day_of_year;
        return 0;
    }

    /* Check if day has changed */
    if (day_of_year != g_current_day) {
        g_current_day = day_of_year;

        /* Generate new filename */
        char new_path[512];
        generate_dated_filename(g_base_path, new_path, sizeof(new_path));

        /* Get log sites for dictionary */
        uint32_t num_sites = 0;
        const log_site_t* sites = log_registry_get_all(&g_registry, &num_sites);

        /* Get custom levels */
        uint32_t num_custom_levels = 0;
        const custom_level_t* custom_levels = _cnanolog_get_custom_levels(&num_custom_levels);

        /* Rotate the log file */
        fprintf(stderr, "cnanolog: Rotating log file to: %s\n", new_path);
        if (binwriter_rotate(g_binary_writer, new_path, sites, num_sites,
                           (const custom_level_entry_t*)custom_levels, num_custom_levels,
#ifndef CNANOLOG_NO_TIMESTAMPS
                           g_timestamp_frequency,
                           g_start_timestamp,
                           g_start_time_sec,
                           g_start_time_nsec
#else
                           0, 0, now, 0
#endif
                          ) != 0) {
            fprintf(stderr, "cnanolog: Failed to rotate log file\n");
            return -1;
        }
    }

    return 0;
}

/* ============================================================================
 * Initialization
 * ============================================================================ */

int cnanolog_init(const char* log_file_path) {
    if (g_is_initialized) {
        return 0;  /* Already initialized */
    }

    /* Initialize with no rotation */
    g_rotation_policy = CNANOLOG_ROTATE_NONE;

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
        binwriter_close(g_binary_writer, NULL, 0, NULL, 0);
        log_registry_destroy(&g_registry);
        return -1;
    }

    /* Initialize buffer registry */
    buffer_registry_init(&g_buffer_registry);

    /* Start background writer thread */
    if (cnanolog_thread_create(&g_writer_thread, writer_thread_main, NULL) != 0) {
        fprintf(stderr, "cnanolog_init: Failed to create writer thread\n");
        binwriter_close(g_binary_writer, NULL, 0, NULL, 0);
        log_registry_destroy(&g_registry);
        return -1;
    }

    g_is_initialized = 1;
    return 0;
}

int cnanolog_init_ex(const cnanolog_rotation_config_t* config) {
    if (config == NULL) {
        fprintf(stderr, "cnanolog_init_ex: config is NULL\n");
        return -1;
    }

    if (g_is_initialized) {
        return 0;  /* Already initialized */
    }

    /* Store rotation configuration */
    g_rotation_policy = config->policy;
    if (config->base_path != NULL) {
        strncpy(g_base_path, config->base_path, sizeof(g_base_path) - 1);
        g_base_path[sizeof(g_base_path) - 1] = '\0';
    }

    /* Generate dated filename if rotation is enabled */
    char log_file_path[512];
    if (g_rotation_policy == CNANOLOG_ROTATE_DAILY) {
        generate_dated_filename(g_base_path, log_file_path, sizeof(log_file_path));
        fprintf(stderr, "cnanolog: Starting with log file: %s\n", log_file_path);
    } else {
        strncpy(log_file_path, g_base_path, sizeof(log_file_path) - 1);
        log_file_path[sizeof(log_file_path) - 1] = '\0';
    }

    /* Initialize registry */
    log_registry_init(&g_registry);

    /* Create binary writer */
    g_binary_writer = binwriter_create(log_file_path);
    if (g_binary_writer == NULL) {
        fprintf(stderr, "cnanolog_init_ex: Failed to create binary writer\n");
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
        fprintf(stderr, "cnanolog_init_ex: Failed to write header\n");
        binwriter_close(g_binary_writer, NULL, 0, NULL, 0);
        log_registry_destroy(&g_registry);
        return -1;
    }

    /* Initialize buffer registry */
    buffer_registry_init(&g_buffer_registry);

    /* Initialize current day for rotation */
    if (g_rotation_policy == CNANOLOG_ROTATE_DAILY) {
        time_t now = time(NULL);
        struct tm* tm = localtime(&now);
        g_current_day = tm->tm_yday;
    }

    /* Start background writer thread */
    if (cnanolog_thread_create(&g_writer_thread, writer_thread_main, NULL) != 0) {
        fprintf(stderr, "cnanolog_init_ex: Failed to create writer thread\n");
        binwriter_close(g_binary_writer, NULL, 0, NULL, 0);
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

    /* Get custom levels */
    uint32_t num_custom_levels = 0;
    const custom_level_t* custom_levels = _cnanolog_get_custom_levels(&num_custom_levels);

    /* Close binary writer (writes dictionary) */
    if (binwriter_close(g_binary_writer, sites, num_sites,
                      (const custom_level_entry_t*)custom_levels, num_custom_levels) != 0) {
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
    if (unlikely(!g_is_initialized || log_id == UINT32_MAX)) {
        return;
    }

#if !defined(CNANOLOG_NO_TIMESTAMPS) && !defined(CNANOLOG_NO_STATISTICS)
    g_stats.total_logs++;
#endif

    staging_buffer_t* sb = get_or_create_staging_buffer();
    if (unlikely(sb == NULL)) {
#if !defined(CNANOLOG_NO_TIMESTAMPS) && !defined(CNANOLOG_NO_STATISTICS)
        g_stats.dropped_logs++;
#endif
        return;  /* Failed to allocate buffer */
    }

    /* Calculate exact size for fixed-size args, early exit for strings */
    size_t reserve_size = sizeof(cnanolog_entry_header_t);

    for (uint8_t i = 0; i < num_args; i++) {
        if (arg_types[i] == ARG_TYPE_STRING) {
            reserve_size = MAX_LOG_ENTRY_SIZE;
            break;
        }

        switch (arg_types[i]) {
            case ARG_TYPE_INT32:
            case ARG_TYPE_UINT32:
                reserve_size += 4;
                break;
            case ARG_TYPE_INT64:
            case ARG_TYPE_UINT64:
            case ARG_TYPE_DOUBLE:
            case ARG_TYPE_POINTER:
                reserve_size += 8;
                break;
        }
    }

    char* write_ptr = staging_reserve(sb, reserve_size);
    if (unlikely(write_ptr == NULL)) {
#if !defined(CNANOLOG_NO_TIMESTAMPS) && !defined(CNANOLOG_NO_STATISTICS)
        g_stats.dropped_logs++;
#endif
        return;
    }

    cnanolog_entry_header_t* header = (cnanolog_entry_header_t*)write_ptr;
    header->log_id = log_id;
#ifndef CNANOLOG_NO_TIMESTAMPS
    header->timestamp = get_timestamp();
#endif

    size_t arg_data_size = 0;
    if (num_args > 0) {
        va_list args;
        va_start(args, arg_types);
        char* arg_data = write_ptr + sizeof(cnanolog_entry_header_t);
        arg_data_size = arg_pack_write_fast(arg_data,
                                             reserve_size - sizeof(cnanolog_entry_header_t),
                                             num_args, arg_types, args);
        va_end(args);

        if (unlikely(arg_data_size == 0)) {
            staging_adjust_reservation(sb, reserve_size, 0);
#if !defined(CNANOLOG_NO_TIMESTAMPS) && !defined(CNANOLOG_NO_STATISTICS)
            g_stats.dropped_logs++;
#endif
            return;
        }
    }

    header->data_length = (uint16_t)arg_data_size;
    size_t actual_entry_size = sizeof(cnanolog_entry_header_t) + arg_data_size;

    /* Only adjust if we reserved pessimistically (MAX_LOG_ENTRY_SIZE) */
    if (reserve_size == MAX_LOG_ENTRY_SIZE && actual_entry_size != reserve_size) {
        staging_adjust_reservation(sb, reserve_size, actual_entry_size);
    }
    staging_commit(sb, actual_entry_size);
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

#if !defined(CNANOLOG_NO_TIMESTAMPS) && !defined(CNANOLOG_NO_STATISTICS)
        g_stats.background_wakeups++;
#endif

        size_t num_buffers = g_buffer_registry.count;

        for (size_t i = 0; i < num_buffers; i++) {
            size_t idx = (last_checked_idx + i) % num_buffers;

#if defined(__GNUC__) || defined(__clang__)
            staging_buffer_t* sb = __atomic_load_n(&g_buffer_registry.buffers[idx], __ATOMIC_ACQUIRE);
#else
            staging_buffer_t* sb = g_buffer_registry.buffers[idx];
#endif
            if (sb == NULL) {
                continue;
            }

            size_t available = staging_available(sb);
            if (available == 0) {
                continue;
            }

            while (staging_available(sb) >= sizeof(cnanolog_entry_header_t)) {
                size_t nread = staging_read(sb, temp_buf, sizeof(cnanolog_entry_header_t));
                if (nread < sizeof(cnanolog_entry_header_t)) {
                    break;
                }

                cnanolog_entry_header_t* header = (cnanolog_entry_header_t*)temp_buf;

                if (header->log_id == STAGING_WRAP_MARKER_LOG_ID) {
                    staging_consume(sb, sizeof(cnanolog_entry_header_t));
                    staging_wrap_read_pos(sb);
                    continue;
                }

                size_t entry_size = sizeof(cnanolog_entry_header_t) + header->data_length;

                if (staging_available(sb) < entry_size) {
                    break;
                }

                nread = staging_read(sb, temp_buf, entry_size);
                if (nread < entry_size) {
                    break;
                }

                header = (cnanolog_entry_header_t*)temp_buf;
                const log_site_t* site = log_registry_get(&g_registry, header->log_id);

                size_t compressed_len = 0;
                const char* data_to_write;
                uint16_t data_len_to_write;

                if (site != NULL && site->num_args > 0) {
                    int compress_result = compress_entry_args(
                        temp_buf + sizeof(cnanolog_entry_header_t),
                        header->data_length,
                        compressed_buf,
                        &compressed_len,
                        site);

                    if (compress_result == 0) {
                        data_to_write = compressed_buf;
                        data_len_to_write = (uint16_t)compressed_len;
#if !defined(CNANOLOG_NO_TIMESTAMPS) && !defined(CNANOLOG_NO_STATISTICS)
                        g_stats.bytes_compressed_from += header->data_length;
                        g_stats.bytes_compressed_to += compressed_len;
#endif
                    } else {
                        data_to_write = temp_buf + sizeof(cnanolog_entry_header_t);
                        data_len_to_write = header->data_length;
                    }
                } else {
                    data_to_write = temp_buf + sizeof(cnanolog_entry_header_t);
                    data_len_to_write = header->data_length;
                }

                binwriter_write_entry(g_binary_writer,
                                    header->log_id,
#ifndef CNANOLOG_NO_TIMESTAMPS
                                    header->timestamp,
#else
                                    0,  /* No timestamp */
#endif
                                    data_to_write,
                                    data_len_to_write);

                staging_consume(sb, entry_size);
                entries_since_flush++;
                found_work = 1;
            }
        }

        if (num_buffers > 0) {
            last_checked_idx = (last_checked_idx + 1) % num_buffers;
        }

#ifndef CNANOLOG_NO_TIMESTAMPS
        uint64_t now = get_timestamp();
        uint64_t elapsed_ns = now - last_flush_time;
        uint64_t elapsed_ms = elapsed_ns / 1000000;

        if (entries_since_flush >= FLUSH_BATCH_SIZE ||
            elapsed_ms >= FLUSH_INTERVAL_MS ||
            (entries_since_flush > 0 && !found_work)) {
#else
        if (entries_since_flush >= FLUSH_BATCH_SIZE ||
            (entries_since_flush > 0 && !found_work)) {
#endif
            binwriter_flush(g_binary_writer);
            entries_since_flush = 0;
#ifndef CNANOLOG_NO_TIMESTAMPS
            last_flush_time = now;
#endif
        }

        /* Check if rotation is needed (once per loop iteration) */
        if (g_rotation_policy != CNANOLOG_ROTATE_NONE) {
            check_and_rotate_if_needed();
        }

        if (!found_work) {
            struct timespec ts = {0, 100000};  /* 100us */
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
}

static int buffer_registry_add(buffer_registry_t* registry, staging_buffer_t* buffer) {
#if defined(__GNUC__) || defined(__clang__)
    uint32_t idx = __atomic_fetch_add(&registry->count, 1, __ATOMIC_SEQ_CST);
#else
    uint32_t idx = registry->count++;
#endif

    if (unlikely(idx >= MAX_STAGING_BUFFERS)) {
        fprintf(stderr, "cnanolog: Buffer registry full (max %d threads)\n", MAX_STAGING_BUFFERS);
        return -1;
    }

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

static staging_buffer_t* get_or_create_staging_buffer(void) {
    if (tls_staging_buffer != NULL) {
        return tls_staging_buffer;
    }

#if defined(__GNUC__) || defined(__clang__)
    uint32_t thread_id = __atomic_fetch_add(&g_next_thread_id, 1, __ATOMIC_SEQ_CST);
#else
    uint32_t thread_id = g_next_thread_id++;
#endif

    staging_buffer_t* sb = staging_buffer_create(thread_id);
    if (sb == NULL) {
        fprintf(stderr, "cnanolog: Failed to allocate staging buffer for thread %u\n", thread_id);
        return NULL;
    }

    if (buffer_registry_add(&g_buffer_registry, sb) != 0) {
        fprintf(stderr, "cnanolog: Failed to register staging buffer\n");
        staging_buffer_destroy(sb);
        return NULL;
    }

    tls_staging_buffer = sb;
    return sb;
}

/* ============================================================================
 * Timestamp
 * ============================================================================ */

#ifndef CNANOLOG_NO_TIMESTAMPS
static uint64_t get_timestamp(void) {
    return rdtsc();
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
    stats->total_logs_written = g_stats.total_logs;
    stats->dropped_logs = g_stats.dropped_logs;
    stats->background_wakeups = g_stats.background_wakeups;

    if (g_binary_writer != NULL) {
        stats->total_bytes_written = binwriter_get_bytes_written(g_binary_writer);
    } else {
        stats->total_bytes_written = 0;
    }

    if (g_stats.bytes_compressed_from > 0) {
        stats->compression_ratio_x100 =
            (g_stats.bytes_compressed_from * 100) / g_stats.bytes_compressed_to;
    } else {
        stats->compression_ratio_x100 = 100;
    }

    stats->staging_buffers_active = g_buffer_registry.count;
#else
    memset(stats, 0, sizeof(*stats));
#endif
}

void cnanolog_reset_stats(void) {
#if !defined(CNANOLOG_NO_TIMESTAMPS) && !defined(CNANOLOG_NO_STATISTICS)
    g_stats.total_logs = 0;
    g_stats.dropped_logs = 0;
    g_stats.bytes_written = 0;
    g_stats.bytes_compressed_from = 0;
    g_stats.bytes_compressed_to = 0;
    g_stats.background_wakeups = 0;
#endif
}

void cnanolog_preallocate(void) {
    staging_buffer_t* sb = get_or_create_staging_buffer();
    (void)sb;
}

int cnanolog_set_writer_affinity(int core_id) {
    if (!g_is_initialized) {
        fprintf(stderr, "cnanolog_set_writer_affinity: Logger not initialized\n");
        return -1;
    }

    if (core_id < 0) {
        fprintf(stderr, "cnanolog_set_writer_affinity: Invalid core ID %d\n", core_id);
        return -1;
    }

    return cnanolog_thread_set_affinity(g_writer_thread, core_id);
}
