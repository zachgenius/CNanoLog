/* Copyright (c) 2025
 * CNanoLog Text Formatter
 *
 * Formats binary log entries to human-readable text.
 * Runs in background thread, keeping producer threads fast.
 */

#pragma once

#include "../include/cnanolog_format.h"
#include "log_registry.h"
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Text Writer Context
 * ============================================================================ */

/**
 * Context for text formatting and writing.
 * Opaque type - implementation details hidden in text_formatter.c.
 */
typedef struct text_writer text_writer_t;

/* ============================================================================
 * Text Writer API
 * ============================================================================ */

/**
 * Create a text writer for the given file.
 * Opens the file in append mode.
 *
 * @param file_path Path to text log file
 * @return Pointer to text writer, or NULL on failure
 */
text_writer_t* text_writer_create(const char* file_path);

/**
 * Set timestamp calibration data (called after rdtsc calibration).
 *
 * @param writer Text writer context
 * @param frequency CPU frequency in Hz
 * @param start_timestamp Initial rdtsc value
 * @param start_time_sec Wall-clock seconds
 * @param start_time_nsec Nanoseconds component
 */
void text_writer_set_timestamp_info(text_writer_t* writer,
                                     uint64_t frequency,
                                     uint64_t start_timestamp,
                                     time_t start_time_sec,
                                     int32_t start_time_nsec);

/**
 * Format and write a log entry to text file.
 * This is called by the background thread for each log entry.
 *
 * @param writer Text writer context
 * @param log_id Log site ID
 * @param timestamp rdtsc timestamp (0 if timestamps disabled)
 * @param arg_data Packed argument data (possibly compressed)
 * @param arg_data_len Length of argument data
 * @param registry Log site registry (for format string lookup)
 * @return 0 on success, -1 on failure
 */
int text_writer_write_entry(text_writer_t* writer,
                             uint32_t log_id,
                             uint64_t timestamp,
                             const char* arg_data,
                             uint16_t arg_data_len,
                             const log_registry_t* registry);

/**
 * Flush buffered data to disk.
 */
void text_writer_flush(text_writer_t* writer);

/**
 * Rotate to a new log file (for log rotation).
 * Closes current file and opens a new one.
 *
 * @param writer Text writer context
 * @param new_path Path to new log file
 * @return 0 on success, -1 on failure
 */
int text_writer_rotate(text_writer_t* writer, const char* new_path);

/**
 * Close the text writer and release resources.
 */
void text_writer_close(text_writer_t* writer);

/**
 * Get total bytes written to file.
 * Used for statistics reporting.
 */
uint64_t text_writer_get_bytes_written(text_writer_t* writer);

#ifdef __cplusplus
}
#endif
