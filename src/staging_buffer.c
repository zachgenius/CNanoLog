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

    /* Initialize non-atomic fields */
    sb->write_pos = 0;
    sb->read_pos = 0;
    sb->thread_id = thread_id;
    sb->active = 1;

    /* Initialize atomic committed field */
    atomic_store_explicit(&sb->committed, 0, memory_order_relaxed);

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
        /* Not enough space at end - attempt wrap-around to beginning */
        size_t space_at_beginning = sb->read_pos;

        /* Can wrap if: space at beginning AND room for wrap marker at end */
        if (space_at_beginning > (nbytes + 64) &&  /* 64-byte safety margin */
            available >= sizeof(cnanolog_entry_header_t)) {

            /* Write wrap marker at current position */
            cnanolog_entry_header_t* wrap_marker =
                (cnanolog_entry_header_t*)(sb->data + sb->write_pos);
            wrap_marker->log_id = STAGING_WRAP_MARKER_LOG_ID;
            wrap_marker->data_length = 0;
#ifndef CNANOLOG_NO_TIMESTAMPS
            wrap_marker->timestamp = 0;
#endif

            size_t wrap_marker_end = sb->write_pos + sizeof(cnanolog_entry_header_t);
            if (wrap_marker_end > STAGING_BUFFER_SIZE) {
                return NULL;
            }

            /* Commit wrap marker atomically (release semantics) */
            atomic_store_explicit(&sb->committed, wrap_marker_end, memory_order_release);

            /* Wrap to beginning and allocate */
            sb->write_pos = 0;
            char* ptr = sb->data;
            sb->write_pos += nbytes;
            return ptr;
        }

        /* Cannot wrap - buffer is full */
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

    /* Atomically publish write_pos to committed (release semantics) */
    atomic_store_explicit(&sb->committed, sb->write_pos, memory_order_release);
}

void staging_adjust_reservation(staging_buffer_t* sb, size_t reserved_bytes, size_t actual_bytes) {
    if (sb == NULL) {
        return;
    }

    /* Sanity check: actual must be <= reserved */
    if (actual_bytes > reserved_bytes) {
        return;
    }

    /* Give back the unused space by reducing write_pos (non-atomic) */
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

    /* Read committed atomically (acquire semantics) */
    size_t committed_pos = atomic_load_explicit(&sb->committed, memory_order_acquire);

    if (committed_pos >= sb->read_pos) {
        return committed_pos - sb->read_pos;
    } else {
        /* Wrap-around case: read to end first, then consumer will wrap */
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

    sb->read_pos += nbytes;

    /* Special case: avoid overflow if read_pos at end and write_pos wrapped */
    size_t committed_pos = atomic_load_explicit(&sb->committed, memory_order_relaxed);
    if (sb->read_pos >= STAGING_BUFFER_SIZE - sizeof(cnanolog_entry_header_t) &&
        sb->read_pos >= committed_pos &&
        sb->write_pos == 0) {
        sb->read_pos = 0;
    }
}

void staging_wrap_read_pos(staging_buffer_t* sb) {
    if (sb == NULL) {
        return;
    }

    sb->read_pos = 0;
}

void staging_reset(staging_buffer_t* sb) {
    if (sb == NULL) {
        return;
    }

    sb->write_pos = 0;
    atomic_store_explicit(&sb->committed, 0, memory_order_relaxed);
    sb->read_pos = 0;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

uint8_t staging_fill_percent(const staging_buffer_t* sb) {
    if (sb == NULL) {
        return 0;
    }

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

    size_t committed_pos = atomic_load_explicit(&sb->committed, memory_order_acquire);
    return (committed_pos == sb->read_pos);
}
