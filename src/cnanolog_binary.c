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
#include "ring_buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_LOG_ENTRY_SIZE 4096

/* ============================================================================
 * Global State
 * ============================================================================ */

static log_registry_t g_registry;
static ring_buffer_t g_ring_buffer;
static mutex_t g_buffer_mutex;
static cond_t g_buffer_cond;
static thread_t g_writer_thread;
static binary_writer_t* g_binary_writer = NULL;
static volatile int g_should_exit = 0;
static int g_is_initialized = 0;

/* Timing for binary logs */
static uint64_t g_start_timestamp = 0;
static time_t g_start_time_sec = 0;
static int32_t g_start_time_nsec = 0;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void* writer_thread_main(void* arg);
static uint64_t get_timestamp(void);

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

    /* Initialize ring buffer and synchronization */
    rb_init(&g_ring_buffer);
    mutex_init(&g_buffer_mutex);
    cond_init(&g_buffer_cond);

    /* Start background writer thread */
    if (thread_create(&g_writer_thread, writer_thread_main, NULL) != 0) {
        fprintf(stderr, "cnanolog_init: Failed to create writer thread\n");
        binwriter_close(g_binary_writer, NULL, 0);
        log_registry_destroy(&g_registry);
        mutex_destroy(&g_buffer_mutex);
        cond_destroy(&g_buffer_cond);
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
    cond_signal(&g_buffer_cond);
    thread_join(g_writer_thread, NULL);

    /* Final flush of ring buffer */
    char temp_buf[MAX_LOG_ENTRY_SIZE];
    mutex_lock(&g_buffer_mutex);
    while (rb_read(&g_ring_buffer, temp_buf, MAX_LOG_ENTRY_SIZE) > 0) {
        /* Parse and write entry */
        cnanolog_entry_header_t* header = (cnanolog_entry_header_t*)temp_buf;
        binwriter_write_entry(g_binary_writer,
                            header->log_id,
                            header->timestamp,
                            temp_buf + sizeof(cnanolog_entry_header_t),
                            header->data_length);
    }
    mutex_unlock(&g_buffer_mutex);

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
    mutex_destroy(&g_buffer_mutex);
    cond_destroy(&g_buffer_cond);

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

    /* Calculate argument data size */
    va_list args, args_copy;
    va_start(args, arg_types);
    va_copy(args_copy, args);
    size_t arg_data_size = arg_pack_calc_size(num_args, arg_types, args_copy);
    va_end(args_copy);

    /* Build entry in temporary buffer */
    char entry_buf[MAX_LOG_ENTRY_SIZE];
    size_t entry_size = sizeof(cnanolog_entry_header_t) + arg_data_size;

    if (entry_size > MAX_LOG_ENTRY_SIZE) {
        /* Entry too large, drop it */
        va_end(args);
        return;
    }

    /* Write entry header */
    cnanolog_entry_header_t* header = (cnanolog_entry_header_t*)entry_buf;
    header->log_id = log_id;
    header->timestamp = get_timestamp();
    header->data_length = (uint16_t)arg_data_size;

    /* Pack arguments */
    char* arg_data = entry_buf + sizeof(cnanolog_entry_header_t);
    arg_pack_write(arg_data, arg_data_size, num_args, arg_types, args);
    va_end(args);

    /* Write to ring buffer */
    mutex_lock(&g_buffer_mutex);
    if (rb_write(&g_ring_buffer, entry_buf, (int)entry_size) != 0) {
        /* Buffer full, drop log (could add statistics here) */
    }
    mutex_unlock(&g_buffer_mutex);

    /* Signal writer thread */
    cond_signal(&g_buffer_cond);
}

/* ============================================================================
 * Background Writer Thread
 * ============================================================================ */

static void* writer_thread_main(void* arg) {
    (void)arg;
    char temp_buf[MAX_LOG_ENTRY_SIZE];

    while (!g_should_exit) {
        mutex_lock(&g_buffer_mutex);

        /* Wait for data or exit signal */
        while (!g_should_exit &&
               g_ring_buffer.read_pos == g_ring_buffer.write_pos &&
               !g_ring_buffer.is_full) {
            cond_wait(&g_buffer_cond, &g_buffer_mutex);
        }

        /* Process all available entries */
        while (rb_read(&g_ring_buffer, temp_buf, MAX_LOG_ENTRY_SIZE) > 0) {
            mutex_unlock(&g_buffer_mutex);

            /* Parse entry */
            cnanolog_entry_header_t* header = (cnanolog_entry_header_t*)temp_buf;

            /* Write to binary file */
            binwriter_write_entry(g_binary_writer,
                                header->log_id,
                                header->timestamp,
                                temp_buf + sizeof(cnanolog_entry_header_t),
                                header->data_length);

            mutex_lock(&g_buffer_mutex);
        }

        mutex_unlock(&g_buffer_mutex);

        /* Flush to disk periodically */
        binwriter_flush(g_binary_writer);
    }

    return NULL;
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
