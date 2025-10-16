/* Copyright (c) 2025
 * CNanoLog Thread-Local Staging Buffer
 *
 * Per-thread staging buffer with lock-free producer operations.
 * Each logging thread writes to its own buffer, eliminating global mutex contention.
 */

#pragma once

#include "platform.h"
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Compatibility macro for static assertions */
#ifdef __cplusplus
    #define CNANOLOG_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
    #define CNANOLOG_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * Staging buffer size per thread (12MB default).
 * Larger size = better burst handling but more memory per thread.
 * Memory usage = STAGING_BUFFER_SIZE × number of logging threads
 */
#define STAGING_BUFFER_SIZE (12 * 1024 * 1024)

/**
 * Wrap marker log_id value.
 * When the producer reaches the end of the buffer and needs to wrap around,
 * it writes a special entry with this log_id to signal the consumer.
 * The consumer detects this marker and resets read_pos to 0.
 *
 * This enables true circular buffer behavior with immediate space reclamation.
 */
#define STAGING_WRAP_MARKER_LOG_ID 0xFFFFFFFF

/* ============================================================================
 * Staging Buffer Structure
 * ============================================================================ */

/**
 * Thread-local staging buffer for lock-free logging.
 * Single-producer, single-consumer model with cache-line optimization.
 *
 * Producer: Reserves space (write_pos), writes data, commits atomically.
 * Consumer: Reads committed entries (read_pos to committed), consumes data.
 *
 * Layout: Hot fields at FRONT (better cache locality), data buffer at END.
 */
typedef struct ALIGN_CACHELINE {
    /* Producer cache line - only written by logging thread */
    size_t write_pos;
    char _pad1[CACHE_LINE_SIZE - sizeof(size_t)];

    /* Extra padding for maximum separation (128 bytes total) */
    char _pad2[CACHE_LINE_SIZE];

    /* Consumer-dominated cache line - atomic, written by producer on commit, read by consumer frequently */
    atomic_size_t committed;
    char _pad3[CACHE_LINE_SIZE - sizeof(atomic_size_t)];

    /* Consumer cache line - only written by background thread */
    size_t read_pos;
    uint32_t thread_id;
    uint8_t active;
    char _pad4[CACHE_LINE_SIZE - sizeof(size_t) - sizeof(uint32_t) - sizeof(uint8_t)];

    /* Buffer storage - at end to keep hot fields close to struct base */
    char data[STAGING_BUFFER_SIZE];
} staging_buffer_t;

/* Compile-time verification of cache-line alignment */
CNANOLOG_STATIC_ASSERT(sizeof(staging_buffer_t) % CACHE_LINE_SIZE == 0,
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
 * Reserve space in the staging buffer for writing (lock-free, O(1)).
 *
 * @param sb Staging buffer
 * @param nbytes Number of bytes to reserve
 * @return Pointer to reserved space, or NULL if buffer is full
 */
char* staging_reserve(staging_buffer_t* sb, size_t nbytes);

/**
 * Commit previously reserved space (makes data visible to consumer).
 * Uses atomic store with release semantics.
 *
 * @param sb Staging buffer
 * @param nbytes Number of bytes to commit (must match staging_reserve)
 */
void staging_commit(staging_buffer_t* sb, size_t nbytes);

/**
 * Adjust reservation before commit (if you reserved more than needed).
 *
 * @param sb Staging buffer
 * @param reserved_bytes Original reserved amount
 * @param actual_bytes Actual amount used
 */
void staging_adjust_reservation(staging_buffer_t* sb, size_t reserved_bytes, size_t actual_bytes);

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
 * Read data from the staging buffer without consuming it (peek operation).
 * Data remains in buffer until staging_consume() is called.
 *
 * @param sb Staging buffer
 * @param out Output buffer to copy data into
 * @param max_len Maximum bytes to read
 * @return Number of bytes actually read
 */
size_t staging_read(staging_buffer_t* sb, char* out, size_t max_len);

/**
 * Mark bytes as consumed, freeing space in the buffer.
 *
 * @param sb Staging buffer
 * @param nbytes Number of bytes to mark as consumed
 */
void staging_consume(staging_buffer_t* sb, size_t nbytes);

/**
 * Wrap read_pos to beginning of buffer.
 * Called by consumer when it detects a wrap marker (log_id == STAGING_WRAP_MARKER_LOG_ID).
 *
 * @param sb Staging buffer
 */
void staging_wrap_read_pos(staging_buffer_t* sb);

/**
 * Reset the staging buffer to empty state.
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
