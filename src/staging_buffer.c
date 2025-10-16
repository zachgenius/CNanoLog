/* Copyright (c) 2025
 * CNanoLog Thread-Local Staging Buffer Implementation
 */

#include "staging_buffer.h"
#include "../include/cnanolog_format.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Platform-Specific Memory Fence
 * ============================================================================ */

/**
 * Memory fence to ensure writes complete before updating committed pointer.
 * This is crucial for the lock-free producer-consumer model.
 */
static inline void memory_fence_release(void) {
#if defined(__GNUC__) || defined(__clang__)
    /* GCC/Clang: Use atomic thread fence */
    __atomic_thread_fence(__ATOMIC_RELEASE);
#elif defined(_MSC_VER)
    /* MSVC: Use MemoryBarrier intrinsic */
    MemoryBarrier();
#else
    /* Fallback: Compiler barrier (not as strong but better than nothing) */
    __asm__ __volatile__("" ::: "memory");
#endif
}

static inline void memory_fence_acquire(void) {
#if defined(__GNUC__) || defined(__clang__)
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
#elif defined(_MSC_VER)
    MemoryBarrier();
#else
    __asm__ __volatile__("" ::: "memory");
#endif
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

staging_buffer_t* staging_buffer_create(uint32_t thread_id) {
    staging_buffer_t* sb = (staging_buffer_t*)malloc(sizeof(staging_buffer_t));
    if (sb == NULL) {
        return NULL;
    }

    memset(sb, 0, sizeof(staging_buffer_t));
    sb->write_pos = 0;
    sb->committed = 0;
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

    /* Check if we have enough space at current position */
    size_t available = STAGING_BUFFER_SIZE - sb->write_pos;

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
                (cnanolog_entry_header_t*)(sb->data + sb->write_pos);

            wrap_marker->log_id = STAGING_WRAP_MARKER_LOG_ID;
            wrap_marker->data_length = 0;
#ifndef CNANOLOG_NO_TIMESTAMPS
            wrap_marker->timestamp = 0;
#endif

            /* Step 2: Calculate end position of wrap marker */
            size_t wrap_marker_end = sb->write_pos + sizeof(cnanolog_entry_header_t);

            /* Verify wrap marker doesn't overflow buffer */
            if (wrap_marker_end > STAGING_BUFFER_SIZE) {
                /* Not enough space for wrap marker - cannot wrap */
                return NULL;
            }

            /* Step 3: Memory fence to ensure wrap marker is written */
            memory_fence_release();

            /*
             * Step 4: Commit wrap marker.
             * This makes the wrap marker visible to consumer before we wrap.
             */
            sb->committed = wrap_marker_end;

            /* Step 5: Wrap write_pos to beginning */
            sb->write_pos = 0;

            /* Step 6: Allocate at beginning */
            char* ptr = sb->data + sb->write_pos;
            sb->write_pos += nbytes;

            return ptr;
        }

        /* Cannot wrap - buffer is truly full */
        return NULL;
    }

    /* Enough space at current position - normal allocation */
    char* ptr = sb->data + sb->write_pos;
    sb->write_pos += nbytes;

    return ptr;
}

void staging_commit(staging_buffer_t* sb, size_t nbytes) {
    if (sb == NULL || nbytes == 0) {
        return;
    }

    /*
     * Memory fence ensures all writes to the reserved region complete
     * before we update the committed pointer.
     *
     * This is the key to the lock-free design:
     * - Producer writes data
     * - Memory fence (release semantics)
     * - Update committed pointer
     * - Consumer sees updated committed pointer (acquire semantics)
     * - Consumer's reads are guaranteed to see all producer writes
     */
    memory_fence_release();

    /* Update committed pointer - now visible to consumer */
    sb->committed = sb->write_pos;
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
        sb->write_pos -= unused;
    }
}

/* ============================================================================
 * Consumer API
 * ============================================================================ */

size_t staging_available(const staging_buffer_t* sb) {
    if (sb == NULL) {
        return 0;
    }

    /* Ensure we see the latest committed value */
    memory_fence_acquire();

    if (sb->committed >= sb->read_pos) {
        /* Normal case: committed is ahead of read_pos */
        return sb->committed - sb->read_pos;
    } else {
        /*
         * Wrap-around case: committed < read_pos
         * This means the producer wrapped to the beginning.
         * Consumer should read from read_pos to end of buffer first,
         * where it will find a wrap marker and wrap read_pos to 0.
         * After wrapping, it can then read from 0 to committed.
         */
        return STAGING_BUFFER_SIZE - sb->read_pos;
    }
}

size_t staging_read(staging_buffer_t* sb, char* out, size_t max_len) {
    if (sb == NULL || out == NULL || max_len == 0) {
        return 0;
    }

    /* Ensure we see the latest committed value */
    memory_fence_acquire();

    /* Calculate how many bytes we can read */
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
    if (sb->read_pos >= STAGING_BUFFER_SIZE - sizeof(cnanolog_entry_header_t) &&
        sb->read_pos == sb->committed &&
        sb->write_pos == 0) {
        /*
         * Special case: read_pos is at end, write_pos wrapped to 0,
         * and everything is consumed. Reset to avoid read_pos overflow.
         */
        sb->read_pos = 0;
        sb->committed = 0;
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

    /* If committed pointer was also at the end, wrap it too */
    if (sb->committed >= STAGING_BUFFER_SIZE - sizeof(cnanolog_entry_header_t)) {
        sb->committed = 0;
    }
}

void staging_reset(staging_buffer_t* sb) {
    if (sb == NULL) {
        return;
    }

    sb->write_pos = 0;
    sb->committed = 0;
    sb->read_pos = 0;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

uint8_t staging_fill_percent(const staging_buffer_t* sb) {
    if (sb == NULL) {
        return 0;
    }

    /* Calculate fill percentage based on write_pos */
    return (uint8_t)((sb->write_pos * 100) / STAGING_BUFFER_SIZE);
}

int staging_is_full(const staging_buffer_t* sb) {
    if (sb == NULL) {
        return 0;
    }

    return (sb->write_pos >= STAGING_BUFFER_SIZE);
}

int staging_is_empty(const staging_buffer_t* sb) {
    if (sb == NULL) {
        return 1;
    }

    memory_fence_acquire();
    return (sb->committed == sb->read_pos);
}
