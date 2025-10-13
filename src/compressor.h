/* Copyright (c) 2025
 * CNanoLog Entry Compressor
 *
 * Compress log entry arguments using variable-byte integer encoding.
 * Integers are compressed, strings are kept as-is.
 */

#pragma once

#include "log_registry.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Compression API
 * ============================================================================ */

/**
 * Compress log entry argument data.
 *
 * Takes uncompressed argument data (as packed by arg_packing.h) and compresses
 * integers using variable-byte encoding. Strings are copied as-is.
 *
 * Compressed format:
 *   [Nibbles: N/2 bytes]  ← Compression metadata for integers
 *   [Packed Integers]     ← Variable-byte encoded
 *   [Strings]             ← Length + data (uncompressed)
 *
 * @param uncompressed Uncompressed argument data
 * @param uncompressed_len Length of uncompressed data
 * @param compressed Output buffer for compressed data
 * @param compressed_len Output: length of compressed data
 * @param site Log site information (argument types)
 * @return 0 on success, -1 on error
 */
int compress_entry_args(const char* uncompressed,
                        size_t uncompressed_len,
                        char* compressed,
                        size_t* compressed_len,
                        const log_site_t* site);

/**
 * Calculate maximum size needed for compressed data.
 * Worst case: all integers are 8 bytes + nibble overhead + strings unchanged.
 *
 * @param site Log site information
 * @param uncompressed_len Length of uncompressed data
 * @return Maximum bytes needed for compressed output
 */
size_t compress_max_size(const log_site_t* site, size_t uncompressed_len);

/**
 * Count number of non-string arguments (for nibble calculation).
 *
 * @param site Log site information
 * @return Number of integer/double/pointer arguments
 */
int count_non_string_args(const log_site_t* site);

#ifdef __cplusplus
}
#endif
