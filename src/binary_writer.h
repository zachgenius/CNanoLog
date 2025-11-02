/* Copyright (c) 2025
 * CNanoLog Binary Writer
 *
 * Provides buffered writing of binary log files according to the
 * format specification in docs/BINARY_FORMAT_SPEC.md
 */

#pragma once

#include "../include/cnanolog_format.h"
#include "../include/cnanolog.h"
#include "log_registry.h"
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * Size of internal write buffer.
 *
 * NanoLog uses 64MB and achieves p99.9 = 702ns.
 * Large buffer = very infrequent flushes = excellent tail latency.
 *
 * Memory usage: BINARY_WRITER_BUFFER_SIZE * 2 (double buffering for AIO)
 * Total: 128MB (2 x 64MB buffers)
 */
#define BINARY_WRITER_BUFFER_SIZE (64 * 1024 * 1024)  // 64MB (128MB total)

/**
 * Periodic flush interval (flush count).
 * Setting this to 0 disables periodic flushing (maximum performance).
 * Current setting: 0 (relies on OS buffering, sync on shutdown only)
 */
#define BINARY_WRITER_PERIODIC_FLUSH_COUNT 0

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * Opaque binary writer handle.
 * Contains file handle, write buffer, and state tracking.
 */
typedef struct binary_writer binary_writer_t;

/* ============================================================================
 * Writer Lifecycle
 * ============================================================================ */

/**
 * Create a new binary writer and open the log file.
 *
 * @param path Path to the log file to create/open
 * @return Binary writer handle on success, NULL on failure
 *
 * Note: If file exists, it will be truncated. The file header is written
 * with placeholder values that will be updated in binwriter_close().
 */
binary_writer_t* binwriter_create(const char* path);

/**
 * Write the file header with timing calibration data.
 * This should be called immediately after binwriter_create().
 *
 * @param writer Binary writer handle
 * @param timestamp_frequency CPU ticks per second (from rdtsc calibration)
 * @param start_timestamp rdtsc() value when logging started
 * @param start_time_sec Unix epoch seconds when logging started
 * @param start_time_nsec Nanoseconds component (0-999999999)
 * @return 0 on success, -1 on failure
 */
int binwriter_write_header(binary_writer_t* writer,
                            uint64_t timestamp_frequency,
                            uint64_t start_timestamp,
                            time_t start_time_sec,
                            int32_t start_time_nsec);

/**
 * Write a log entry to the file.
 * Entries are buffered and written in batches for efficiency.
 *
 * @param writer Binary writer handle
 * @param log_id Log site identifier
 * @param timestamp rdtsc() timestamp when log was created
 * @param arg_data Pointer to argument data (raw binary)
 * @param data_len Length of argument data in bytes
 * @return 0 on success, -1 on failure
 *
 * Note: The caller is responsible for formatting arg_data according to
 * the log site's argument types.
 */
int binwriter_write_entry(binary_writer_t* writer,
                           uint32_t log_id,
                           uint64_t timestamp,
                           const void* arg_data,
                           uint16_t data_len);

/**
 * Flush the internal buffer to disk.
 * This is called automatically when the buffer fills, but can be called
 * explicitly to ensure data is written (e.g., before shutdown).
 *
 * @param writer Binary writer handle
 * @return 0 on success, -1 on failure
 */
int binwriter_flush(binary_writer_t* writer);

/**
 * Write the dictionary, update the file header, and close the file.
 * After this call, the writer handle is invalid and should not be used.
 *
 * @param writer Binary writer handle
 * @param sites Array of log site information
 * @param num_sites Number of log sites in the array
 * @return 0 on success, -1 on failure
 *
 * Note: This function:
 * 1. Flushes any remaining buffered data
 * 2. Writes the dictionary at the current position
 * 3. Seeks back to the header and updates entry_count
 * 4. Closes the file and frees the writer
 */
int binwriter_close(binary_writer_t* writer,
                    const log_site_t* sites,
                    uint32_t num_sites);

/**
 * Rotate the log file (for date-based rotation).
 * Closes the current file and opens a new one.
 *
 * @param writer Binary writer handle
 * @param new_path Path to the new log file
 * @param sites Array of log site information (for dictionary)
 * @param num_sites Number of log sites
 * @param timestamp_frequency CPU ticks per second
 * @param start_timestamp rdtsc() value when logging started
 * @param start_time_sec Unix epoch seconds
 * @param start_time_nsec Nanoseconds component
 * @return 0 on success, -1 on failure
 *
 * Note: This function:
 * 1. Flushes and closes the current file with dictionary
 * 2. Opens a new file at new_path
 * 3. Writes a new file header
 * 4. Resets entry count for the new file
 */
int binwriter_rotate(binary_writer_t* writer,
                     const char* new_path,
                     const log_site_t* sites,
                     uint32_t num_sites,
                     uint64_t timestamp_frequency,
                     uint64_t start_timestamp,
                     time_t start_time_sec,
                     int32_t start_time_nsec);

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * Get the current number of entries written.
 *
 * @param writer Binary writer handle
 * @return Number of log entries written so far
 */
uint32_t binwriter_get_entry_count(const binary_writer_t* writer);

/**
 * Get the total number of bytes written to the file (excluding buffer).
 *
 * @param writer Binary writer handle
 * @return Number of bytes written to disk
 */
uint64_t binwriter_get_bytes_written(const binary_writer_t* writer);

/**
 * Get the number of bytes currently buffered (not yet written to disk).
 *
 * @param writer Binary writer handle
 * @return Number of bytes in the internal buffer
 */
size_t binwriter_get_buffered_bytes(const binary_writer_t* writer);

#ifdef __cplusplus
}
#endif
