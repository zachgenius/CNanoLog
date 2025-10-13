/* Copyright (c) 2025
 * CNanoLog Entry Compressor Implementation
 */

#include "compressor.h"
#include "packer.h"
#include "../include/cnanolog_format.h"
#include <string.h>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

int count_non_string_args(const log_site_t* site) {
    int count = 0;
    for (uint8_t i = 0; i < site->num_args; i++) {
        if (site->arg_types[i] != ARG_TYPE_STRING) {
            count++;
        }
    }
    return count;
}

size_t compress_max_size(const log_site_t* site, size_t uncompressed_len) {
    /* Worst case: nibbles + all data unchanged */
    int num_int_args = count_non_string_args(site);
    size_t nibble_size = nibble_bytes(num_int_args);
    return nibble_size + uncompressed_len;
}

/* ============================================================================
 * Two-Pass Compression
 * ============================================================================ */

int compress_entry_args(const char* uncompressed,
                        size_t uncompressed_len,
                        char* compressed,
                        size_t* compressed_len,
                        const log_site_t* site) {

    if (!uncompressed || !compressed || !compressed_len || !site) {
        return -1;
    }

    const char* read_ptr = uncompressed;
    char* write_ptr = compressed;

    /* Calculate and reserve space for nibbles */
    int num_int_args = count_non_string_args(site);
    size_t nibble_size = nibble_bytes(num_int_args);
    uint8_t* nibbles = (uint8_t*)write_ptr;
    memset(nibbles, 0, nibble_size);
    write_ptr += nibble_size;

    int nibble_idx = 0;

    /* ==================================================================
     * PASS 1: Pack integers using variable-byte encoding
     * ================================================================== */

    for (uint8_t i = 0; i < site->num_args; i++) {
        switch (site->arg_types[i]) {
            case ARG_TYPE_INT32: {
                int32_t val;
                memcpy(&val, read_ptr, sizeof(int32_t));
                read_ptr += sizeof(int32_t);

                int is_negative;
                uint8_t num_bytes = pack_int32(&write_ptr, val, &is_negative);

                /* Store nibble: high bit = sign, low 4 bits = size */
                uint8_t nibble = num_bytes | (is_negative ? 0x08 : 0x00);
                set_nibble(nibbles, nibble_idx++, nibble);
                break;
            }

            case ARG_TYPE_INT64: {
                int64_t val;
                memcpy(&val, read_ptr, sizeof(int64_t));
                read_ptr += sizeof(int64_t);

                int is_negative;
                uint8_t num_bytes = pack_int64(&write_ptr, val, &is_negative);

                uint8_t nibble = num_bytes | (is_negative ? 0x08 : 0x00);
                set_nibble(nibbles, nibble_idx++, nibble);
                break;
            }

            case ARG_TYPE_UINT32: {
                uint32_t val;
                memcpy(&val, read_ptr, sizeof(uint32_t));
                read_ptr += sizeof(uint32_t);

                uint8_t num_bytes = pack_uint32(&write_ptr, val);
                set_nibble(nibbles, nibble_idx++, num_bytes);
                break;
            }

            case ARG_TYPE_UINT64: {
                uint64_t val;
                memcpy(&val, read_ptr, sizeof(uint64_t));
                read_ptr += sizeof(uint64_t);

                uint8_t num_bytes = pack_uint64(&write_ptr, val);
                set_nibble(nibbles, nibble_idx++, num_bytes);
                break;
            }

            case ARG_TYPE_DOUBLE: {
                /* Doubles: store as-is (no compression) */
                memcpy(write_ptr, read_ptr, sizeof(double));
                read_ptr += sizeof(double);
                write_ptr += sizeof(double);

                /* Nibble = 8 (always 8 bytes for double) */
                set_nibble(nibbles, nibble_idx++, 8);
                break;
            }

            case ARG_TYPE_POINTER: {
                /* Pointers: compress as uint64 */
                uint64_t val;
                memcpy(&val, read_ptr, sizeof(uint64_t));
                read_ptr += sizeof(uint64_t);

                uint8_t num_bytes = pack_uint64(&write_ptr, val);
                set_nibble(nibbles, nibble_idx++, num_bytes);
                break;
            }

            case ARG_TYPE_STRING: {
                /* Skip strings in pass 1 */
                uint32_t len;
                memcpy(&len, read_ptr, sizeof(uint32_t));
                read_ptr += sizeof(uint32_t) + len;
                break;
            }

            default:
                break;
        }
    }

    /* ==================================================================
     * PASS 2: Copy strings as-is
     * ================================================================== */

    read_ptr = uncompressed;  /* Reset read pointer */

    for (uint8_t i = 0; i < site->num_args; i++) {
        if (site->arg_types[i] == ARG_TYPE_STRING) {
            /* Copy length + string data */
            uint32_t len;
            memcpy(&len, read_ptr, sizeof(uint32_t));
            read_ptr += sizeof(uint32_t);

            memcpy(write_ptr, &len, sizeof(uint32_t));
            write_ptr += sizeof(uint32_t);

            if (len > 0) {
                memcpy(write_ptr, read_ptr, len);
                write_ptr += len;
            }

            read_ptr += len;
        } else {
            /* Skip non-strings (already processed in pass 1) */
            switch (site->arg_types[i]) {
                case ARG_TYPE_INT32:  read_ptr += sizeof(int32_t); break;
                case ARG_TYPE_INT64:  read_ptr += sizeof(int64_t); break;
                case ARG_TYPE_UINT32: read_ptr += sizeof(uint32_t); break;
                case ARG_TYPE_UINT64: read_ptr += sizeof(uint64_t); break;
                case ARG_TYPE_DOUBLE: read_ptr += sizeof(double); break;
                case ARG_TYPE_POINTER: read_ptr += sizeof(uint64_t); break;
                default: break;
            }
        }
    }

    *compressed_len = write_ptr - compressed;
    return 0;
}
