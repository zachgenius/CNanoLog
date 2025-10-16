/* Copyright (c) 2025
 * CNanoLog Thread-Local Staging Buffer Implementation
 */

#include "staging_buffer.h"
#include "../include/cnanolog_format.h"
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

staging_buffer_t* staging_buffer_create(uint32_t thread_id) {
    staging_buffer_t* sb = (staging_buffer_t*)malloc(sizeof(staging_buffer_t));
    if (sb == NULL) {
        return NULL;
    }

    /* Zero out the entire structure first */
    memset(sb, 0, sizeof(staging_buffer_t));

    /* Initialize atomic fields - use atomic_store instead of atomic_init
     * since memset already zeroed it */
    atomic_store_explicit(&sb->write_pos, 0, memory_order_relaxed);

    /* Initialize non-atomic fields */
    sb->read_pos = 0;
    sb->thread_id = thread_id;
    sb->active = 1;

    return sb;
}

void staging_buffer_destroy(staging_buffer_t* sb) {
    if (sb != NULL) {
        free(sb);
    }
}

/* ============================================================================
 * Producer API (Lock-Free)
 * ============================================================================ */

char* staging_reserve(staging_buffer_t* sb, size_t nbytes) {
    if (sb == NULL || nbytes == 0) {
        return NULL;
    }

    /* Get current write position (relaxed, only producer modifies this) */
    size_t current_write_pos = atomic_load_explicit(&sb->write_pos, memory_order_relaxed);

    /* Check if we have enough space at current position */
    size_t available = STAGING_BUFFER_SIZE - current_write_pos;

    if (nbytes > available) {
        /*
         * Not enough space at end of buffer - attempt wrap-around.
         *
         * Circular buffer strategy:
         * - If consumer has advanced (read_pos > 0), we can wrap to beginning
         * - Write wrap marker at current position to signal consumer
         * - Reset write_pos to 0 and try to allocate there
         */

        /* Check space available from beginning (0 to read_pos) */
        size_t space_at_beginning = sb->read_pos;

        /*
         * Wrap-around conditions:
         * 1. Enough space at beginning for requested allocation (with safety margin)
         * 2. Enough space at end to write full wrap marker header
         *    (Critical: must not overflow buffer!)
         */
        if (space_at_beginning > (nbytes + 64) &&  /* 64-byte safety margin */
            available >= sizeof(cnanolog_entry_header_t)) {
            /*
             * Wrap around is possible!
             *
             * Step 1: Write wrap marker at current position.
             * The marker looks like a regular entry with log_id = 0xFFFFFFFF.
             * Consumer will detect this and wrap read_pos to 0.
             */
            cnanolog_entry_header_t* wrap_marker =
                (cnanolog_entry_header_t*)(sb->data + current_write_pos);

            wrap_marker->log_id = STAGING_WRAP_MARKER_LOG_ID;
            wrap_marker->data_length = 0;
#ifndef CNANOLOG_NO_TIMESTAMPS
            wrap_marker->timestamp = 0;
#endif

            /* Step 2: Calculate end position of wrap marker */
            size_t wrap_marker_end = current_write_pos + sizeof(cnanolog_entry_header_t);

            /* Verify wrap marker doesn't overflow buffer */
            if (wrap_marker_end > STAGING_BUFFER_SIZE) {
                /* Not enough space for wrap marker - cannot wrap */
                return NULL;
            }

            /*
             * Step 3: Atomically commit wrap marker with release semantics.
             * This makes the wrap marker visible to consumer before we wrap.
             * Release semantics ensure all previous writes (wrap marker data) complete first.
             */
            atomic_store_explicit(&sb->write_pos, wrap_marker_end, memory_order_release);

            /* Step 4: Allocate at beginning (use relaxed, only producer modifies) */
            char* ptr = sb->data;
            atomic_store_explicit(&sb->write_pos, nbytes, memory_order_relaxed);

            return ptr;
        }

        /* Cannot wrap - buffer is truly full */
        return NULL;
    }

    /* Enough space at current position - normal allocation */
    char* ptr = sb->data + current_write_pos;
    atomic_store_explicit(&sb->write_pos, current_write_pos + nbytes, memory_order_relaxed);

    return ptr;
}

void staging_commit(staging_buffer_t* sb, size_t nbytes) {
    if (sb == NULL || nbytes == 0) {
        return;
    }

    /*
     * Make write_pos visible to consumer with release semantics.
     *
     * This is the key to the lock-free design:
     * - Producer writes data to reserved space
     * - Producer updated write_pos with relaxed stores in staging_reserve()
     * - This atomic_store with release semantics ensures all previous writes
     *   (both the data and the relaxed write_pos stores) become visible
     * - Consumer reads write_pos with acquire semantics
     * - Consumer's reads are guaranteed to see all producer writes
     *
     * We read the current write_pos and store it back with release semantics
     * to establish the synchronization point. This "publishes" the tentative
     * write_pos value that was set in staging_reserve().
     */
    size_t current_pos = atomic_load_explicit(&sb->write_pos, memory_order_relaxed);
    atomic_store_explicit(&sb->write_pos, current_pos, memory_order_release);
}

void staging_adjust_reservation(staging_buffer_t* sb, size_t reserved_bytes, size_t actual_bytes) {
    if (sb == NULL) {
        return;
    }

    /* Sanity check: actual must be <= reserved */
    if (actual_bytes > reserved_bytes) {
        return;
    }

    /* Give back the unused space by reducing write_pos */
    size_t unused = reserved_bytes - actual_bytes;
    if (unused > 0) {
        size_t current_pos = atomic_load_explicit(&sb->write_pos, memory_order_relaxed);
        atomic_store_explicit(&sb->write_pos, current_pos - unused, memory_order_relaxed);
    }
}

/* ============================================================================
 * Consumer API
 * ============================================================================ */

size_t staging_available(const staging_buffer_t* sb) {
    if (sb == NULL) {
        return 0;
    }

    /*
     * Read write_pos atomically with acquire semantics.
     * This synchronizes with the release store in staging_commit(),
     * ensuring we see all the producer's writes to the data buffer.
     */
    size_t write_position = atomic_load_explicit(&sb->write_pos, memory_order_acquire);

    if (write_position >= sb->read_pos) {
        /* Normal case: write_pos is ahead of read_pos */
        return write_position - sb->read_pos;
    } else {
        /*
         * Wrap-around case: write_pos < read_pos
         * This means the producer wrapped to the beginning.
         * Consumer should read from read_pos to end of buffer first,
         * where it will find a wrap marker and wrap read_pos to 0.
         * After wrapping, it can then read from 0 to write_pos.
         */
        return STAGING_BUFFER_SIZE - sb->read_pos;
    }
}

size_t staging_read(staging_buffer_t* sb, char* out, size_t max_len) {
    if (sb == NULL || out == NULL || max_len == 0) {
        return 0;
    }

    /* Calculate how many bytes we can read (includes acquire fence) */
    size_t available = staging_available(sb);
    if (available == 0) {
        return 0;
    }

    /* Read up to max_len or available, whichever is smaller */
    size_t to_read = (available < max_len) ? available : max_len;

    /* Copy data to output buffer */
    memcpy(out, sb->data + sb->read_pos, to_read);

    return to_read;
}

void staging_consume(staging_buffer_t* sb, size_t nbytes) {
    if (sb == NULL || nbytes == 0) {
        return;
    }

    /* Advance read position */
    sb->read_pos += nbytes;

    /*
     * Note: With circular buffer, we don't automatically reset positions.
     * Reset happens explicitly when:
     * 1. Consumer detects wrap marker and calls staging_wrap_read_pos()
     * 2. Buffer is completely empty and both positions are at end
     */
    size_t current_write_pos = atomic_load_explicit(&sb->write_pos, memory_order_relaxed);
    if (sb->read_pos >= STAGING_BUFFER_SIZE - sizeof(cnanolog_entry_header_t) &&
        sb->read_pos >= current_write_pos &&
        current_write_pos == 0) {
        /*
         * Special case: read_pos is at end, write_pos wrapped to 0,
         * and everything is consumed. Reset to avoid read_pos overflow.
         */
        sb->read_pos = 0;
    }
}

/**
 * Wrap read_pos to beginning of buffer.
 * Called by consumer when it detects a wrap marker (log_id == 0xFFFFFFFF).
 *
 * @param sb Staging buffer
 */
void staging_wrap_read_pos(staging_buffer_t* sb) {
    if (sb == NULL) {
        return;
    }

    /* Wrap read position to beginning */
    sb->read_pos = 0;

    /*
     * Note: We no longer need to wrap write_pos - the producer has already
     * wrapped it atomically when it wrote the wrap marker. The consumer just
     * needs to update its read position.
     */
}

void staging_reset(staging_buffer_t* sb) {
    if (sb == NULL) {
        return;
    }

    atomic_store_explicit(&sb->write_pos, 0, memory_order_relaxed);
    sb->read_pos = 0;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

uint8_t staging_fill_percent(const staging_buffer_t* sb) {
    if (sb == NULL) {
        return 0;
    }

    /* Calculate fill percentage based on write_pos (relaxed read is fine for stats) */
    size_t write_position = atomic_load_explicit(&sb->write_pos, memory_order_relaxed);
    return (uint8_t)((write_position * 100) / STAGING_BUFFER_SIZE);
}

int staging_is_full(const staging_buffer_t* sb) {
    if (sb == NULL) {
        return 0;
    }

    size_t write_position = atomic_load_explicit(&sb->write_pos, memory_order_relaxed);
    return (write_position >= STAGING_BUFFER_SIZE);
}

int staging_is_empty(const staging_buffer_t* sb) {
    if (sb == NULL) {
        return 1;
    }

    /* Read write_pos with acquire semantics for consistency */
    size_t write_position = atomic_load_explicit(&sb->write_pos, memory_order_acquire);
    return (write_position == sb->read_pos);
}
