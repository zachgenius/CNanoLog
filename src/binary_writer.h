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
 * Larger buffers reduce flush frequency and improve tail latency:
 * - 64KB:  Good for memory-constrained systems (more frequent flushes)
 * - 512KB: Balanced (recommended for most use cases)
 * - 4MB:   Better tail latency (fewer aio_write() calls)
 * - 16MB:  Best tail latency (matches high-performance systems)
 *
 * Trade-off: Memory usage = BINARY_WRITER_BUFFER_SIZE * 2 (double buffering)
 * NanoLog uses 64MB buffer and achieves p99.9 = 702ns
 */
#define BINARY_WRITER_BUFFER_SIZE (4 * 1024 * 1024)  // 4MB (8MB total with double buffer)

/**
 * Periodic flush interval (flush count).
 *
 * The binary writer will call fflush() once every N buffer flushes to ensure
 * data durability while maintaining high performance during bursts.
 *
 * Setting this to 0 disables periodic flushing.
 * They rely on OS buffering during operation and only sync on shutdown.
 *
 * Calculation:
 * - Buffer size = 64KB per flush
 * - Flush every N flushes = N * 64KB of data at risk
 * - Example: 100 flushes = 6.4MB at risk
 *
 * Recommended values:
 * - 0: Maximum performance, relies on OS buffering
 * - 10-50: Aggressive flushing, ~640KB-3.2MB at risk
 * - 100-200: Good balance, ~6.4MB-12.8MB at risk
 * - 500+: Maximum performance, more data at risk
 *
 * Note: Using flush count instead of time avoids time() syscalls in hot path.
 * Current setting: 0 (no periodic flushing for maximum performance)
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
