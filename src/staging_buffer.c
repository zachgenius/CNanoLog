/* Copyright (c) 2025
 * CNanoLog Thread-Local Staging Buffer Implementation
 */

#include "staging_buffer.h"
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

    /* Check if we have enough space */
    size_t available = STAGING_BUFFER_SIZE - sb->write_pos;
    if (nbytes > available) {
        /* Not enough space - buffer is full */
        return NULL;
    }

    /* Reserve space by returning pointer and advancing write_pos */
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

/* ============================================================================
 * Consumer API
 * ============================================================================ */

size_t staging_available(const staging_buffer_t* sb) {
    if (sb == NULL) {
        return 0;
    }

    /* Ensure we see the latest committed value */
    memory_fence_acquire();

    /* Available bytes = committed - read_pos */
    if (sb->committed >= sb->read_pos) {
        return sb->committed - sb->read_pos;
    }

    /* This should never happen in correct usage */
    return 0;
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
     * Optimization: If we've consumed everything, reset the buffer.
     * This prevents the buffer from filling up and needing reallocation.
     */
    if (sb->read_pos >= sb->committed && sb->read_pos == sb->write_pos) {
        sb->read_pos = 0;
        sb->write_pos = 0;
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
