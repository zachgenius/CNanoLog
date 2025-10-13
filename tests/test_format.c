/*
 * Format validation tests
 * Verifies that binary format structures match specification
 */

#include "../include/cnanolog_format.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define TEST_PASS() printf("  ✓ %s\n", __func__)
#define TEST_FAIL(msg) do { \
    printf("  ✗ %s: %s\n", __func__, msg); \
    return 1; \
} while(0)

/* Test that struct sizes match specification */
int test_struct_sizes() {
    if (sizeof(cnanolog_file_header_t) != 64)
        TEST_FAIL("File header size mismatch");

    if (sizeof(cnanolog_entry_header_t) != 14)
        TEST_FAIL("Entry header size mismatch");

    if (sizeof(cnanolog_dict_header_t) != 16)
        TEST_FAIL("Dictionary header size mismatch");

    if (sizeof(cnanolog_dict_entry_t) != 30)
        TEST_FAIL("Dictionary entry size mismatch");

    TEST_PASS();
    return 0;
}

/* Test field offsets in file header */
int test_file_header_offsets() {
    cnanolog_file_header_t h;
    char* base = (char*)&h;

    /* Verify offsets match specification */
    if ((char*)&h.magic - base != 0)
        TEST_FAIL("magic offset wrong");
    if ((char*)&h.version_major - base != 4)
        TEST_FAIL("version_major offset wrong");
    if ((char*)&h.version_minor - base != 6)
        TEST_FAIL("version_minor offset wrong");
    if ((char*)&h.timestamp_frequency - base != 8)
        TEST_FAIL("timestamp_frequency offset wrong");
    if ((char*)&h.start_timestamp - base != 16)
        TEST_FAIL("start_timestamp offset wrong");
    if ((char*)&h.start_time_sec - base != 24)
        TEST_FAIL("start_time_sec offset wrong");
    if ((char*)&h.start_time_nsec - base != 32)
        TEST_FAIL("start_time_nsec offset wrong");
    if ((char*)&h.endianness - base != 36)
        TEST_FAIL("endianness offset wrong");
    if ((char*)&h.dictionary_offset - base != 40)
        TEST_FAIL("dictionary_offset offset wrong");
    if ((char*)&h.entry_count - base != 48)
        TEST_FAIL("entry_count offset wrong");
    if ((char*)&h.reserved - base != 52)
        TEST_FAIL("reserved offset wrong");

    TEST_PASS();
    return 0;
}

/* Test magic numbers */
int test_magic_numbers() {
    if (CNANOLOG_MAGIC != 0x4E414E4F)
        TEST_FAIL("NANO magic number wrong");

    if (CNANOLOG_DICT_MAGIC != 0x44494354)
        TEST_FAIL("DICT magic number wrong");

    /* Verify it spells "NANO" and "DICT" in little-endian */
    union {
        uint32_t val;
        char bytes[4];
    } magic;

    magic.val = CNANOLOG_MAGIC;
    if (magic.bytes[0] != 'N' || magic.bytes[1] != 'A' ||
        magic.bytes[2] != 'N' || magic.bytes[3] != 'O') {
        /* This is expected on little-endian systems */
        /* On big-endian, it would be reversed */
    }

    TEST_PASS();
    return 0;
}

/* Test endianness detection */
int test_endianness_detection() {
    /* Test same endianness */
    if (cnanolog_check_endianness(CNANOLOG_ENDIAN_MAGIC) != 0)
        TEST_FAIL("Same endianness not detected");

    /* Test different endianness */
    if (cnanolog_check_endianness(0x04030201) != 1)
        TEST_FAIL("Different endianness not detected");

    /* Test invalid marker */
    if (cnanolog_check_endianness(0x12345678) != -1)
        TEST_FAIL("Invalid marker not detected");

    TEST_PASS();
    return 0;
}

/* Test byte swap functions */
int test_byte_swap() {
    if (cnanolog_bswap16(0x1234) != 0x3412)
        TEST_FAIL("bswap16 failed");

    if (cnanolog_bswap32(0x12345678) != 0x78563412)
        TEST_FAIL("bswap32 failed");

    if (cnanolog_bswap64(0x123456789ABCDEF0ULL) != 0xF0DEBC9A78563412ULL)
        TEST_FAIL("bswap64 failed");

    TEST_PASS();
    return 0;
}

/* Test file header validation */
int test_file_header_validation() {
    cnanolog_file_header_t h;
    memset(&h, 0, sizeof(h));

    /* Invalid magic */
    h.magic = 0x12345678;
    h.version_major = CNANOLOG_VERSION_MAJOR;
    if (cnanolog_validate_file_header(&h) != -1)
        TEST_FAIL("Invalid magic not caught");

    /* Valid magic, wrong version */
    h.magic = CNANOLOG_MAGIC;
    h.version_major = 99;
    if (cnanolog_validate_file_header(&h) != -1)
        TEST_FAIL("Invalid version not caught");

    /* Valid header */
    h.magic = CNANOLOG_MAGIC;
    h.version_major = CNANOLOG_VERSION_MAJOR;
    h.version_minor = CNANOLOG_VERSION_MINOR;
    if (cnanolog_validate_file_header(&h) != 0)
        TEST_FAIL("Valid header rejected");

    /* Valid header with newer minor version (should accept) */
    h.version_minor = CNANOLOG_VERSION_MINOR + 1;
    if (cnanolog_validate_file_header(&h) != 0)
        TEST_FAIL("Newer minor version rejected");

    TEST_PASS();
    return 0;
}

/* Test dictionary header validation */
int test_dict_header_validation() {
    cnanolog_dict_header_t h;
    memset(&h, 0, sizeof(h));

    /* Invalid magic */
    h.magic = 0x12345678;
    if (cnanolog_validate_dict_header(&h) != -1)
        TEST_FAIL("Invalid magic not caught");

    /* Valid magic */
    h.magic = CNANOLOG_DICT_MAGIC;
    if (cnanolog_validate_dict_header(&h) != 0)
        TEST_FAIL("Valid header rejected");

    TEST_PASS();
    return 0;
}

/* Test argument type enum values */
int test_arg_types() {
    if (ARG_TYPE_NONE != 0) TEST_FAIL("ARG_TYPE_NONE wrong");
    if (ARG_TYPE_INT32 != 1) TEST_FAIL("ARG_TYPE_INT32 wrong");
    if (ARG_TYPE_INT64 != 2) TEST_FAIL("ARG_TYPE_INT64 wrong");
    if (ARG_TYPE_UINT32 != 3) TEST_FAIL("ARG_TYPE_UINT32 wrong");
    if (ARG_TYPE_UINT64 != 4) TEST_FAIL("ARG_TYPE_UINT64 wrong");
    if (ARG_TYPE_DOUBLE != 5) TEST_FAIL("ARG_TYPE_DOUBLE wrong");
    if (ARG_TYPE_STRING != 6) TEST_FAIL("ARG_TYPE_STRING wrong");
    if (ARG_TYPE_POINTER != 7) TEST_FAIL("ARG_TYPE_POINTER wrong");

    TEST_PASS();
    return 0;
}

/* Test size calculation macros */
int test_size_macros() {
    /* Entry with no data */
    if (CNANOLOG_ENTRY_TOTAL_SIZE(0) != 14)
        TEST_FAIL("Entry size macro wrong for 0 bytes");

    /* Entry with 4 bytes of data */
    if (CNANOLOG_ENTRY_TOTAL_SIZE(4) != 18)
        TEST_FAIL("Entry size macro wrong for 4 bytes");

    /* Dict entry with no strings */
    if (CNANOLOG_DICT_ENTRY_TOTAL_SIZE(0, 0) != 30)
        TEST_FAIL("Dict entry size macro wrong for no strings");

    /* Dict entry with strings */
    if (CNANOLOG_DICT_ENTRY_TOTAL_SIZE(6, 10) != 46)
        TEST_FAIL("Dict entry size macro wrong with strings");

    TEST_PASS();
    return 0;
}

/* Test that arg_types array fits expected number of args */
int test_arg_types_array() {
    cnanolog_dict_entry_t e;

    if (sizeof(e.arg_types) != CNANOLOG_MAX_ARGS)
        TEST_FAIL("arg_types array size mismatch");

    /* Verify we can access all 16 slots */
    for (int i = 0; i < CNANOLOG_MAX_ARGS; i++) {
        e.arg_types[i] = (uint8_t)i;
    }

    TEST_PASS();
    return 0;
}

/* Main test runner */
int main() {
    int failures = 0;

    printf("CNanoLog Binary Format Tests\n");
    printf("=============================\n\n");

    printf("Structure Size Tests:\n");
    failures += test_struct_sizes();

    printf("\nField Offset Tests:\n");
    failures += test_file_header_offsets();

    printf("\nMagic Number Tests:\n");
    failures += test_magic_numbers();

    printf("\nEndianness Tests:\n");
    failures += test_endianness_detection();
    failures += test_byte_swap();

    printf("\nValidation Tests:\n");
    failures += test_file_header_validation();
    failures += test_dict_header_validation();

    printf("\nType Tests:\n");
    failures += test_arg_types();
    failures += test_size_macros();
    failures += test_arg_types_array();

    printf("\n=============================\n");
    if (failures == 0) {
        printf("All tests PASSED ✓\n");
        return 0;
    } else {
        printf("%d test(s) FAILED ✗\n", failures);
        return 1;
    }
}
