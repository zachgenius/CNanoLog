/* Copyright (c) 2025
 * CNanoLog Variable-Byte Integer Packer Implementation
 */

#include "packer.h"
#include <string.h>

/* ============================================================================
 * Packing Implementation
 * ============================================================================ */

uint8_t pack_uint64(char** buffer, uint64_t val) {
    uint8_t num_bytes;

    /* Determine minimum bytes needed to represent the value */
    if (val < (1ULL << 8))       num_bytes = 1;
    else if (val < (1ULL << 16)) num_bytes = 2;
    else if (val < (1ULL << 24)) num_bytes = 3;
    else if (val < (1ULL << 32)) num_bytes = 4;
    else if (val < (1ULL << 40)) num_bytes = 5;
    else if (val < (1ULL << 48)) num_bytes = 6;
    else if (val < (1ULL << 56)) num_bytes = 7;
    else                         num_bytes = 8;

    /* Copy only the needed bytes (little-endian) */
    memcpy(*buffer, &val, num_bytes);
    *buffer += num_bytes;

    return num_bytes;
}

uint8_t pack_int64(char** buffer, int64_t val, int* is_negative) {
    if (val >= 0) {
        /* Positive value: pack as unsigned */
        *is_negative = 0;
        return pack_uint64(buffer, (uint64_t)val);
    } else {
        /* Negative value: pack absolute value */
        *is_negative = 1;
        return pack_uint64(buffer, (uint64_t)(-val));
    }
}

/* ============================================================================
 * Unpacking Implementation
 * ============================================================================ */

uint64_t unpack_uint64(const char** buffer, uint8_t num_bytes) {
    uint64_t val = 0;

    /* Validate num_bytes */
    if (num_bytes == 0 || num_bytes > 8) {
        return 0;  /* Invalid */
    }

    /* Copy the bytes and zero-extend to 64 bits */
    memcpy(&val, *buffer, num_bytes);
    *buffer += num_bytes;

    return val;
}

int64_t unpack_int64(const char** buffer, uint8_t num_bytes, int is_negative) {
    /* Unpack as unsigned first */
    uint64_t abs_val = unpack_uint64(buffer, num_bytes);

    /* Apply sign */
    if (is_negative) {
        return -(int64_t)abs_val;
    } else {
        return (int64_t)abs_val;
    }
}
