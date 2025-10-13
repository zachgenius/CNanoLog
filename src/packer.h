/* Copyright (c) 2025
 * CNanoLog Variable-Byte Integer Packer
 *
 * Compress integers using variable-byte encoding to save space.
 * Small values use fewer bytes:
 *   0-255         → 1 byte (75% savings vs uint32)
 *   256-65535     → 2 bytes (50% savings vs uint32)
 *   etc.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Packing API (Compression)
 * ============================================================================ */

/**
 * Pack an unsigned 64-bit integer using variable-byte encoding.
 *
 * @param buffer Pointer to buffer pointer (will be advanced)
 * @param val Value to pack
 * @return Number of bytes used (1-8)
 *
 * Example:
 *   char buf[8];
 *   char* ptr = buf;
 *   uint8_t nbytes = pack_uint64(&ptr, 42);  // Uses 1 byte
 */
uint8_t pack_uint64(char** buffer, uint64_t val);

/**
 * Pack a signed 64-bit integer using variable-byte encoding.
 * Negative values are handled efficiently.
 *
 * @param buffer Pointer to buffer pointer (will be advanced)
 * @param val Value to pack
 * @param is_negative Output parameter: 1 if negative, 0 if positive
 * @return Number of bytes used (1-8)
 *
 * Note: Caller should store is_negative flag separately (e.g., in nibble high bit)
 */
uint8_t pack_int64(char** buffer, int64_t val, int* is_negative);

/**
 * Pack an unsigned 32-bit integer.
 * Convenience wrapper around pack_uint64.
 */
static inline uint8_t pack_uint32(char** buffer, uint32_t val) {
    return pack_uint64(buffer, val);
}

/**
 * Pack a signed 32-bit integer.
 * Convenience wrapper around pack_int64.
 */
static inline uint8_t pack_int32(char** buffer, int32_t val, int* is_negative) {
    return pack_int64(buffer, val, is_negative);
}

/* ============================================================================
 * Unpacking API (Decompression)
 * ============================================================================ */

/**
 * Unpack an unsigned 64-bit integer.
 *
 * @param buffer Pointer to buffer pointer (will be advanced)
 * @param num_bytes Number of bytes to read (1-8)
 * @return Unpacked value
 */
uint64_t unpack_uint64(const char** buffer, uint8_t num_bytes);

/**
 * Unpack a signed 64-bit integer.
 *
 * @param buffer Pointer to buffer pointer (will be advanced)
 * @param num_bytes Number of bytes to read (1-8)
 * @param is_negative 1 if value is negative, 0 if positive
 * @return Unpacked value
 */
int64_t unpack_int64(const char** buffer, uint8_t num_bytes, int is_negative);

/**
 * Unpack an unsigned 32-bit integer.
 * Convenience wrapper around unpack_uint64.
 */
static inline uint32_t unpack_uint32(const char** buffer, uint8_t num_bytes) {
    return (uint32_t)unpack_uint64(buffer, num_bytes);
}

/**
 * Unpack a signed 32-bit integer.
 * Convenience wrapper around unpack_int64.
 */
static inline int32_t unpack_int32(const char** buffer, uint8_t num_bytes, int is_negative) {
    return (int32_t)unpack_int64(buffer, num_bytes, is_negative);
}

/* ============================================================================
 * Nibble Helper Functions
 * ============================================================================ */

/**
 * Set a 4-bit nibble at the given index.
 *
 * @param nibbles Array of bytes containing nibbles
 * @param idx Nibble index (0 = first nibble, 1 = second nibble, etc.)
 * @param value 4-bit value to store (0-15)
 *
 * Layout: nibbles[0] = [nibble1:4bits][nibble0:4bits]
 *         nibbles[1] = [nibble3:4bits][nibble2:4bits]
 */
static inline void set_nibble(uint8_t* nibbles, int idx, uint8_t value) {
    int byte_idx = idx / 2;
    int shift = (idx % 2) * 4;

    nibbles[byte_idx] &= ~(0x0F << shift);  /* Clear nibble */
    nibbles[byte_idx] |= ((value & 0x0F) << shift);  /* Set nibble */
}

/**
 * Get a 4-bit nibble at the given index.
 *
 * @param nibbles Array of bytes containing nibbles
 * @param idx Nibble index (0 = first nibble, 1 = second nibble, etc.)
 * @return 4-bit value (0-15)
 */
static inline uint8_t get_nibble(const uint8_t* nibbles, int idx) {
    int byte_idx = idx / 2;
    int shift = (idx % 2) * 4;
    return (nibbles[byte_idx] >> shift) & 0x0F;
}

/**
 * Calculate number of bytes needed to store nibbles.
 *
 * @param num_nibbles Number of nibbles (4-bit values)
 * @return Number of bytes needed (rounded up)
 */
static inline size_t nibble_bytes(int num_nibbles) {
    return (num_nibbles + 1) / 2;
}

#ifdef __cplusplus
}
#endif
