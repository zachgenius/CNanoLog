/* Test compression/decompression of multi-argument entries */

#include "../src/compressor.h"
#include "../src/packer.h"
#include "../src/log_registry.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Decompression function from decompressor.c - copied here for testing */
static int count_non_string_args_test(const log_site_t* site) {
    int count = 0;
    for (uint8_t i = 0; i < site->num_args; i++) {
        if (site->arg_types[i] != ARG_TYPE_STRING) {
            count++;
        }
    }
    return count;
}

static int decompress_entry_args_test(const char* compressed,
                                       size_t compressed_len,
                                       char* uncompressed,
                                       size_t uncompressed_size,
                                       const log_site_t* site) {
    const char* read_ptr = compressed;
    char* write_ptr = uncompressed;
    const char* end_ptr = compressed + compressed_len;
    const char* write_end = uncompressed + uncompressed_size;

    /* Calculate nibble size and read nibbles */
    int num_int_args = count_non_string_args_test(site);
    size_t nibble_size = nibble_bytes(num_int_args);

    if (nibble_size > compressed_len) {
        return -1;
    }

    const uint8_t* nibbles = (const uint8_t*)read_ptr;
    read_ptr += nibble_size;

    /* Pass 1: Read all integers into temporary storage */
    uint64_t int_values[16];  // CNANOLOG_MAX_ARGS
    int nibble_idx = 0;
    int int_arg_idx = 0;

    for (uint8_t i = 0; i < site->num_args; i++) {
        switch (site->arg_types[i]) {
            case ARG_TYPE_INT32: {
                if (read_ptr >= end_ptr) return -1;

                uint8_t nibble = get_nibble(nibbles, nibble_idx++);
                uint8_t num_bytes = nibble & 0x07;
                int is_negative = (nibble & 0x08) ? 1 : 0;

                if (num_bytes == 0 || num_bytes > 4) return -1;

                int32_t val = unpack_int32(&read_ptr, num_bytes, is_negative);
                int_values[int_arg_idx++] = (uint64_t)(uint32_t)val;
                break;
            }

            case ARG_TYPE_STRING:
                /* Skip - handled in pass 2 */
                break;

            default:
                return -1;
        }
    }

    /* Pass 2: Write all arguments to uncompressed buffer in order */
    int_arg_idx = 0;  /* Reset for writing */

    for (uint8_t i = 0; i < site->num_args; i++) {
        switch (site->arg_types[i]) {
            case ARG_TYPE_INT32: {
                if (write_ptr + sizeof(int32_t) > write_end) return -1;
                int32_t val = (int32_t)int_values[int_arg_idx++];
                memcpy(write_ptr, &val, sizeof(int32_t));
                write_ptr += sizeof(int32_t);
                break;
            }

            case ARG_TYPE_STRING: {
                /* Read and write string from compressed stream */
                if (read_ptr + sizeof(uint32_t) > end_ptr) return -1;

                uint32_t str_len;
                memcpy(&str_len, read_ptr, sizeof(uint32_t));
                read_ptr += sizeof(uint32_t);

                if (write_ptr + sizeof(uint32_t) > write_end) return -1;
                memcpy(write_ptr, &str_len, sizeof(uint32_t));
                write_ptr += sizeof(uint32_t);

                if (str_len > 0) {
                    if (read_ptr + str_len > end_ptr) return -1;
                    if (write_ptr + str_len > write_end) return -1;

                    memcpy(write_ptr, read_ptr, str_len);
                    read_ptr += str_len;
                    write_ptr += str_len;
                }
                break;
            }

            default:
                return -1;
        }
    }

    /* Validate that we consumed all compressed data */
    if (read_ptr != end_ptr) {
        return -1;
    }

    return (int)(write_ptr - uncompressed);
}

void test_single_int() {
    printf("Test 1: Single int32 (42)...\n");

    // Setup log site
    log_site_t site;
    site.num_args = 1;
    site.arg_types[0] = ARG_TYPE_INT32;

    // Uncompressed: single int32 = 42
    char uncompressed[32];
    int32_t val = 42;
    memcpy(uncompressed, &val, sizeof(int32_t));
    size_t uncompressed_len = sizeof(int32_t);

    // Compress
    char compressed[64];
    size_t compressed_len = 0;
    int result = compress_entry_args(uncompressed, uncompressed_len,
                                      compressed, &compressed_len, &site);

    printf("  Compress result: %d\n", result);
    printf("  Uncompressed size: %zu bytes\n", uncompressed_len);
    printf("  Compressed size: %zu bytes\n", compressed_len);

    // Decompress
    char decompressed[32];
    // We need the decompressor function - let me check if it exists

    // For now, just check compression worked
    if (result == 0) {
        printf("  ✓ Compression succeeded\n");
    } else {
        printf("  ✗ Compression failed\n");
    }
}

void test_two_ints() {
    printf("\nTest 2: Two int32s (100, 200)...\n");

    // Setup log site
    log_site_t site;
    site.num_args = 2;
    site.arg_types[0] = ARG_TYPE_INT32;
    site.arg_types[1] = ARG_TYPE_INT32;

    // Uncompressed: two int32s
    char uncompressed[32];
    int32_t val1 = 100;
    int32_t val2 = 200;
    memcpy(uncompressed, &val1, sizeof(int32_t));
    memcpy(uncompressed + sizeof(int32_t), &val2, sizeof(int32_t));
    size_t uncompressed_len = 2 * sizeof(int32_t);

    printf("  Input: val1=%d, val2=%d\n", val1, val2);

    // Compress
    char compressed[64];
    size_t compressed_len = 0;
    int result = compress_entry_args(uncompressed, uncompressed_len,
                                      compressed, &compressed_len, &site);

    printf("  Compress result: %d\n", result);
    printf("  Uncompressed size: %zu bytes\n", uncompressed_len);
    printf("  Compressed size: %zu bytes\n", compressed_len);

    if (result == 0) {
        // Dump compressed data
        printf("  Compressed data (hex): ");
        for (size_t i = 0; i < compressed_len; i++) {
            printf("%02x ", (unsigned char)compressed[i]);
        }
        printf("\n");

        // Check nibbles
        int num_int_args = 2;
        size_t nibble_size = nibble_bytes(num_int_args);
        printf("  Nibble size: %zu bytes\n", nibble_size);

        const uint8_t* nibbles = (const uint8_t*)compressed;
        for (int i = 0; i < num_int_args; i++) {
            uint8_t nibble = get_nibble(nibbles, i);
            printf("  Nibble[%d]: 0x%x (size=%d, sign=%d)\n",
                   i, nibble, nibble & 0x07, (nibble & 0x08) ? 1 : 0);
        }

        // Decompress
        char decompressed[64];
        int decomp_len = decompress_entry_args_test(compressed, compressed_len,
                                                     decompressed, sizeof(decompressed),
                                                     &site);

        printf("  Decompress result: %d bytes\n", decomp_len);

        if (decomp_len > 0) {
            // Check values
            int32_t out1, out2;
            memcpy(&out1, decompressed, sizeof(int32_t));
            memcpy(&out2, decompressed + sizeof(int32_t), sizeof(int32_t));

            printf("  Output: val1=%d, val2=%d\n", out1, out2);

            if (out1 == val1 && out2 == val2) {
                printf("  ✓ Compression/decompression succeeded!\n");
            } else {
                printf("  ✗ Values mismatch! Expected (%d, %d), got (%d, %d)\n",
                       val1, val2, out1, out2);
            }
        } else {
            printf("  ✗ Decompression failed\n");
        }
    } else {
        printf("  ✗ Compression failed\n");
    }
}

void test_int_and_string() {
    printf("\nTest 3: int32 (500) + string (\"Internal error\")...\n");

    // Setup log site
    log_site_t site;
    site.num_args = 2;
    site.arg_types[0] = ARG_TYPE_INT32;
    site.arg_types[1] = ARG_TYPE_STRING;

    // Uncompressed: int32 + string
    char uncompressed[128];
    char* ptr = uncompressed;

    // Write int32
    int32_t val = 500;
    memcpy(ptr, &val, sizeof(int32_t));
    ptr += sizeof(int32_t);

    // Write string (length + data)
    const char* str = "Internal error";
    uint32_t str_len = strlen(str);
    memcpy(ptr, &str_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, str, str_len);
    ptr += str_len;

    size_t uncompressed_len = ptr - uncompressed;

    printf("  Input: val=%d, str=\"%s\"\n", val, str);

    // Compress
    char compressed[128];
    size_t compressed_len = 0;
    int result = compress_entry_args(uncompressed, uncompressed_len,
                                      compressed, &compressed_len, &site);

    printf("  Compress result: %d\n", result);
    printf("  Uncompressed size: %zu bytes\n", uncompressed_len);
    printf("  Compressed size: %zu bytes\n", compressed_len);

    if (result == 0) {
        // Dump compressed data
        printf("  Compressed data (hex): ");
        for (size_t i = 0; i < compressed_len; i++) {
            printf("%02x ", (unsigned char)compressed[i]);
        }
        printf("\n");

        // Decompress
        char decompressed[128];
        int decomp_len = decompress_entry_args_test(compressed, compressed_len,
                                                     decompressed, sizeof(decompressed),
                                                     &site);

        printf("  Decompress result: %d bytes\n", decomp_len);

        if (decomp_len > 0) {
            // Check int value
            int32_t out_val;
            memcpy(&out_val, decompressed, sizeof(int32_t));

            // Check string
            uint32_t out_str_len;
            memcpy(&out_str_len, decompressed + sizeof(int32_t), sizeof(uint32_t));
            char out_str[64];
            memcpy(out_str, decompressed + sizeof(int32_t) + sizeof(uint32_t), out_str_len);
            out_str[out_str_len] = '\0';

            printf("  Output: val=%d, str=\"%s\"\n", out_val, out_str);

            if (out_val == val && strcmp(out_str, str) == 0) {
                printf("  ✓ Compression/decompression succeeded!\n");
            } else {
                printf("  ✗ Values mismatch!\n");
            }
        } else {
            printf("  ✗ Decompression failed\n");
        }
    } else {
        printf("  ✗ Compression failed\n");
    }
}

void test_three_ints() {
    printf("\nTest 4: Three int32s (10, 20, 30)...\n");

    // Setup log site
    log_site_t site;
    site.num_args = 3;
    site.arg_types[0] = ARG_TYPE_INT32;
    site.arg_types[1] = ARG_TYPE_INT32;
    site.arg_types[2] = ARG_TYPE_INT32;

    // Uncompressed: three int32s
    char uncompressed[32];
    int32_t val1 = 10;
    int32_t val2 = 20;
    int32_t val3 = 30;
    memcpy(uncompressed, &val1, sizeof(int32_t));
    memcpy(uncompressed + sizeof(int32_t), &val2, sizeof(int32_t));
    memcpy(uncompressed + 2*sizeof(int32_t), &val3, sizeof(int32_t));
    size_t uncompressed_len = 3 * sizeof(int32_t);

    printf("  Input: val1=%d, val2=%d, val3=%d\n", val1, val2, val3);

    // Compress
    char compressed[64];
    size_t compressed_len = 0;
    int result = compress_entry_args(uncompressed, uncompressed_len,
                                      compressed, &compressed_len, &site);

    printf("  Compress result: %d\n", result);
    printf("  Uncompressed size: %zu bytes\n", uncompressed_len);
    printf("  Compressed size: %zu bytes\n", compressed_len);

    if (result == 0) {
        // Dump compressed data
        printf("  Compressed data (hex): ");
        for (size_t i = 0; i < compressed_len; i++) {
            printf("%02x ", (unsigned char)compressed[i]);
        }
        printf("\n");

        // Check nibbles
        int num_int_args = 3;
        size_t nibble_size = nibble_bytes(num_int_args);
        printf("  Nibble size: %zu bytes\n", nibble_size);

        const uint8_t* nibbles = (const uint8_t*)compressed;
        for (int i = 0; i < num_int_args; i++) {
            uint8_t nibble = get_nibble(nibbles, i);
            printf("  Nibble[%d]: 0x%x (size=%d, sign=%d)\n",
                   i, nibble, nibble & 0x07, (nibble & 0x08) ? 1 : 0);
        }

        // Decompress
        char decompressed[64];
        int decomp_len = decompress_entry_args_test(compressed, compressed_len,
                                                     decompressed, sizeof(decompressed),
                                                     &site);

        printf("  Decompress result: %d bytes\n", decomp_len);

        if (decomp_len > 0) {
            // Check values
            int32_t out1, out2, out3;
            memcpy(&out1, decompressed, sizeof(int32_t));
            memcpy(&out2, decompressed + sizeof(int32_t), sizeof(int32_t));
            memcpy(&out3, decompressed + 2*sizeof(int32_t), sizeof(int32_t));

            printf("  Output: val1=%d, val2=%d, val3=%d\n", out1, out2, out3);

            if (out1 == val1 && out2 == val2 && out3 == val3) {
                printf("  ✓ Compression/decompression succeeded!\n");
            } else {
                printf("  ✗ Values mismatch! Expected (%d, %d, %d), got (%d, %d, %d)\n",
                       val1, val2, val3, out1, out2, out3);
            }
        } else {
            printf("  ✗ Decompression failed\n");
        }
    } else {
        printf("  ✗ Compression failed\n");
    }
}

int main() {
    printf("CNanoLog Compression Debug Test\n");
    printf("================================\n\n");

    test_single_int();
    test_two_ints();
    test_int_and_string();
    test_three_ints();

    printf("\n================================\n");
    printf("Debug tests complete\n");

    return 0;
}
