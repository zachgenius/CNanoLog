/* Copyright (c) 2025
 * CNanoLog Thread-Local Staging Buffer
 *
 * Per-thread staging buffer with lock-free producer operations.
 * Each logging thread writes to its own buffer, eliminating global mutex contention.
 */

#pragma once

#include "platform.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * Staging buffer size per thread.
 *
 * Tuning guidelines:
 * - 1MB (default): Good for steady logging, ~250K logs buffered
 * - 4MB: Good for moderate bursts, ~1M logs buffered
 * - 8MB: Good for heavy bursts, ~2M logs buffered
 * - 16MB+: For extreme burst scenarios
 *
 * Trade-offs:
 * - Larger = more memory per thread, but fewer drops during bursts
 * - Memory usage = STAGING_BUFFER_SIZE × number of logging threads
 *
 * For burst logging scenarios, 4-8MB is recommended.
 */
#define STAGING_BUFFER_SIZE (8 * 1024 * 1024)

/* ============================================================================
 * Staging Buffer Structure
 * ============================================================================ */

/**
 * Thread-local staging buffer for lock-free logging.
 *
 * Memory Model:
 *   [====== committed ======][--- reserved ---][------- free -------]
 *   ^                        ^                  ^
 *   read_pos                 committed          write_pos
 *
 * Producer (logging thread):
 *   - Increments write_pos to reserve space (no lock)
 *   - Writes data
 *   - Updates committed atomically (with memory fence)
 *
 * Consumer (background thread):
 *   - Reads from read_pos up to committed
 *   - Increments read_pos after consuming
 *
 * Note: Single-producer, single-consumer model (one thread writes, background thread reads)
 *
 * Cache-Line Optimization:
 *   Fields are padded to separate cache lines to avoid false sharing.
 *   - Producer writes write_pos (own cache line)
 *   - Shared committed field (own cache line)
 *   - Consumer writes read_pos (own cache line)
 */
typedef struct ALIGN_CACHELINE {
    /* Buffer storage - naturally aligned at start */
    char data[STAGING_BUFFER_SIZE];

    /* Producer cache line - only written by logging thread */
    size_t write_pos;
    char _pad1[CACHE_LINE_SIZE - sizeof(size_t)];

    /* Shared cache line - written by producer, read by consumer */
    volatile size_t committed;
    char _pad2[CACHE_LINE_SIZE - sizeof(size_t)];

    /* Consumer cache line - written by background thread */
    size_t read_pos;
    uint32_t thread_id;
    uint8_t active;
    char _pad3[CACHE_LINE_SIZE - sizeof(size_t) - sizeof(uint32_t) - sizeof(uint8_t)];
} staging_buffer_t;

/* Compile-time verification of cache-line alignment */
_Static_assert(sizeof(staging_buffer_t) % CACHE_LINE_SIZE == 0,
               "staging_buffer_t size must be multiple of cache line size");

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

/**
 * Create a new staging buffer for a thread.
 *
 * @param thread_id Identifier for the thread (for debugging)
 * @return Pointer to new buffer, or NULL on allocation failure
 */
staging_buffer_t* staging_buffer_create(uint32_t thread_id);

/**
 * Destroy a staging buffer.
 * Should only be called after all data has been consumed.
 *
 * @param sb Staging buffer to destroy
 */
void staging_buffer_destroy(staging_buffer_t* sb);

/* ============================================================================
 * Producer API (Logging Thread - Lock-Free)
 * ============================================================================ */

/**
 * Reserve space in the staging buffer for writing.
 * This operation is lock-free and O(1).
 *
 * @param sb Staging buffer
 * @param nbytes Number of bytes to reserve
 * @return Pointer to reserved space, or NULL if buffer is full
 *
 * Usage:
 *   char* ptr = staging_reserve(sb, 100);
 *   if (ptr) {
 *       memcpy(ptr, data, 100);
 *       staging_commit(sb, 100);
 *   }
 */
char* staging_reserve(staging_buffer_t* sb, size_t nbytes);

/**
 * Commit previously reserved space.
 * Makes the data visible to the consumer (background thread).
 * Includes memory fence to ensure all writes complete before commit.
 *
 * @param sb Staging buffer
 * @param nbytes Number of bytes to commit (must match staging_reserve)
 *
 * Note: Must be called after staging_reserve() with the same nbytes value.
 */
void staging_commit(staging_buffer_t* sb, size_t nbytes);

/* ============================================================================
 * Consumer API (Background Thread)
 * ============================================================================ */

/**
 * Get the number of bytes available to read.
 *
 * @param sb Staging buffer
 * @return Number of bytes that can be read
 */
size_t staging_available(const staging_buffer_t* sb);

/**
 * Read data from the staging buffer without consuming it.
 * This is a "peek" operation - the data remains in the buffer until
 * staging_consume() is called.
 *
 * @param sb Staging buffer
 * @param out Output buffer to copy data into
 * @param max_len Maximum bytes to read
 * @return Number of bytes actually read
 *
 * Note: Data remains in buffer until staging_consume() is called.
 */
size_t staging_read(staging_buffer_t* sb, char* out, size_t max_len);

/**
 * Mark bytes as consumed, freeing space in the buffer.
 * Should be called after staging_read() and processing the data.
 *
 * @param sb Staging buffer
 * @param nbytes Number of bytes to mark as consumed
 */
void staging_consume(staging_buffer_t* sb, size_t nbytes);

/**
 * Reset the staging buffer to empty state.
 * Useful for testing or error recovery.
 *
 * @param sb Staging buffer
 */
void staging_reset(staging_buffer_t* sb);

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * Get the current fill percentage of the buffer.
 *
 * @param sb Staging buffer
 * @return Fill percentage (0-100)
 */
uint8_t staging_fill_percent(const staging_buffer_t* sb);

/**
 * Check if the buffer is full.
 *
 * @param sb Staging buffer
 * @return 1 if full, 0 otherwise
 */
int staging_is_full(const staging_buffer_t* sb);

/**
 * Check if the buffer is empty.
 *
 * @param sb Staging buffer
 * @return 1 if empty, 0 otherwise
 */
int staging_is_empty(const staging_buffer_t* sb);

#ifdef __cplusplus
}
#endif
