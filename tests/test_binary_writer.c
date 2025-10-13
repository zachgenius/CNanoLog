/*
 * Binary Writer Tests
 * Tests the binary writer functionality
 */

#include "../src/binary_writer.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#define TEST_FILE "test_output.clog"
#define TEST_PASS() printf("  ✓ %s\n", __func__)
#define TEST_FAIL(msg) do { \
    printf("  ✗ %s: %s\n", __func__, msg); \
    return 1; \
} while(0)

/* Test creating and closing a writer */
int test_create_close() {
    binary_writer_t* writer = binwriter_create(TEST_FILE);
    if (writer == NULL)
        TEST_FAIL("Failed to create writer");

    /* Write header */
    if (binwriter_write_header(writer, 1000000000ULL, 0, 1234567890, 0) != 0)
        TEST_FAIL("Failed to write header");

    /* Close with empty dictionary */
    if (binwriter_close(writer, NULL, 0) != 0)
        TEST_FAIL("Failed to close writer");

    /* Verify file exists */
    FILE* fp = fopen(TEST_FILE, "rb");
    if (fp == NULL)
        TEST_FAIL("File not created");

    fclose(fp);
    unlink(TEST_FILE);

    TEST_PASS();
    return 0;
}

/* Test writing file header */
int test_write_header() {
    binary_writer_t* writer = binwriter_create(TEST_FILE);
    if (writer == NULL)
        TEST_FAIL("Failed to create writer");

    /* Write header with known values */
    uint64_t freq = 2400000000ULL;  /* 2.4 GHz */
    uint64_t start_ticks = 123456789ULL;
    time_t start_sec = 1700000000;
    int32_t start_nsec = 123456789;

    if (binwriter_write_header(writer, freq, start_ticks, start_sec, start_nsec) != 0)
        TEST_FAIL("Failed to write header");

    /* Flush and close */
    binwriter_close(writer, NULL, 0);

    /* Read back and verify */
    FILE* fp = fopen(TEST_FILE, "rb");
    if (fp == NULL)
        TEST_FAIL("Cannot open file for reading");

    cnanolog_file_header_t header;
    if (fread(&header, 1, sizeof(header), fp) != sizeof(header))
        TEST_FAIL("Failed to read header");

    fclose(fp);
    unlink(TEST_FILE);

    /* Verify fields */
    if (header.magic != CNANOLOG_MAGIC)
        TEST_FAIL("Magic mismatch");
    if (header.version_major != CNANOLOG_VERSION_MAJOR)
        TEST_FAIL("Version major mismatch");
    if (header.version_minor != CNANOLOG_VERSION_MINOR)
        TEST_FAIL("Version minor mismatch");
    if (header.timestamp_frequency != freq)
        TEST_FAIL("Timestamp frequency mismatch");
    if (header.start_timestamp != start_ticks)
        TEST_FAIL("Start timestamp mismatch");
    if (header.start_time_sec != start_sec)
        TEST_FAIL("Start time sec mismatch");
    if (header.start_time_nsec != start_nsec)
        TEST_FAIL("Start time nsec mismatch");
    if (header.endianness != CNANOLOG_ENDIAN_MAGIC)
        TEST_FAIL("Endianness marker wrong");

    TEST_PASS();
    return 0;
}

/* Test writing log entries */
int test_write_entries() {
    binary_writer_t* writer = binwriter_create(TEST_FILE);
    if (writer == NULL)
        TEST_FAIL("Failed to create writer");

    binwriter_write_header(writer, 1000000000ULL, 0, 1234567890, 0);

    /* Write entry with no arguments */
    if (binwriter_write_entry(writer, 0, 100000, NULL, 0) != 0)
        TEST_FAIL("Failed to write entry with no args");

    /* Write entry with one integer argument */
    int32_t value = 42;
    if (binwriter_write_entry(writer, 1, 200000, &value, sizeof(value)) != 0)
        TEST_FAIL("Failed to write entry with int arg");

    /* Check entry count */
    if (binwriter_get_entry_count(writer) != 2)
        TEST_FAIL("Entry count wrong");

    binwriter_close(writer, NULL, 0);

    /* Verify entries in file */
    FILE* fp = fopen(TEST_FILE, "rb");
    if (fp == NULL)
        TEST_FAIL("Cannot open file for reading");

    /* Skip header */
    fseek(fp, sizeof(cnanolog_file_header_t), SEEK_SET);

    /* Read first entry */
    cnanolog_entry_header_t e1;
    if (fread(&e1, 1, sizeof(e1), fp) != sizeof(e1))
        TEST_FAIL("Failed to read entry 1");

    if (e1.log_id != 0 || e1.timestamp != 100000 || e1.data_length != 0)
        TEST_FAIL("Entry 1 data mismatch");

    /* Read second entry */
    cnanolog_entry_header_t e2;
    if (fread(&e2, 1, sizeof(e2), fp) != sizeof(e2))
        TEST_FAIL("Failed to read entry 2");

    if (e2.log_id != 1 || e2.timestamp != 200000 || e2.data_length != 4)
        TEST_FAIL("Entry 2 header mismatch");

    int32_t read_value;
    if (fread(&read_value, 1, sizeof(read_value), fp) != sizeof(read_value))
        TEST_FAIL("Failed to read entry 2 data");

    if (read_value != 42)
        TEST_FAIL("Entry 2 data value mismatch");

    fclose(fp);
    unlink(TEST_FILE);

    TEST_PASS();
    return 0;
}

/* Test writing dictionary */
int test_write_dictionary() {
    binary_writer_t* writer = binwriter_create(TEST_FILE);
    if (writer == NULL)
        TEST_FAIL("Failed to create writer");

    binwriter_write_header(writer, 1000000000ULL, 0, 1234567890, 0);

    /* Write some entries */
    binwriter_write_entry(writer, 0, 100, NULL, 0);
    binwriter_write_entry(writer, 1, 200, NULL, 0);

    /* Prepare dictionary */
    log_site_info_t sites[2];

    sites[0].log_id = 0;
    sites[0].log_level = LOG_LEVEL_INFO;
    sites[0].filename = "test.c";
    sites[0].format = "Hello world";
    sites[0].line_number = 42;
    sites[0].num_args = 0;

    sites[1].log_id = 1;
    sites[1].log_level = LOG_LEVEL_WARN;
    sites[1].filename = "main.c";
    sites[1].format = "Value: %d";
    sites[1].line_number = 100;
    sites[1].num_args = 1;
    sites[1].arg_types[0] = ARG_TYPE_INT32;

    /* Close with dictionary */
    if (binwriter_close(writer, sites, 2) != 0)
        TEST_FAIL("Failed to close with dictionary");

    /* Verify file */
    FILE* fp = fopen(TEST_FILE, "rb");
    if (fp == NULL)
        TEST_FAIL("Cannot open file for reading");

    /* Read and verify header was updated */
    cnanolog_file_header_t header;
    fread(&header, 1, sizeof(header), fp);

    if (header.entry_count != 2)
        TEST_FAIL("Entry count not updated in header");
    if (header.dictionary_offset == 0)
        TEST_FAIL("Dictionary offset not updated");

    /* Seek to dictionary */
    fseek(fp, header.dictionary_offset, SEEK_SET);

    /* Read dictionary header */
    cnanolog_dict_header_t dict_header;
    if (fread(&dict_header, 1, sizeof(dict_header), fp) != sizeof(dict_header))
        TEST_FAIL("Failed to read dictionary header");

    if (dict_header.magic != CNANOLOG_DICT_MAGIC)
        TEST_FAIL("Dictionary magic wrong");
    if (dict_header.num_entries != 2)
        TEST_FAIL("Dictionary entry count wrong");

    /* Read first dict entry */
    cnanolog_dict_entry_t de1;
    if (fread(&de1, 1, sizeof(de1), fp) != sizeof(de1))
        TEST_FAIL("Failed to read dict entry 1");

    if (de1.log_id != 0 || de1.log_level != LOG_LEVEL_INFO || de1.num_args != 0)
        TEST_FAIL("Dict entry 1 fields wrong");
    if (de1.filename_length != 6 || de1.format_length != 11 || de1.line_number != 42)
        TEST_FAIL("Dict entry 1 strings wrong");

    char filename[32], format[32];
    fread(filename, 1, de1.filename_length, fp);
    filename[de1.filename_length] = '\0';
    fread(format, 1, de1.format_length, fp);
    format[de1.format_length] = '\0';

    if (strcmp(filename, "test.c") != 0)
        TEST_FAIL("Dict entry 1 filename mismatch");
    if (strcmp(format, "Hello world") != 0)
        TEST_FAIL("Dict entry 1 format mismatch");

    fclose(fp);
    unlink(TEST_FILE);

    TEST_PASS();
    return 0;
}

/* Test buffer flushing */
int test_buffer_flush() {
    binary_writer_t* writer = binwriter_create(TEST_FILE);
    if (writer == NULL)
        TEST_FAIL("Failed to create writer");

    binwriter_write_header(writer, 1000000000ULL, 0, 1234567890, 0);

    /* Write many entries to trigger buffer flush */
    for (int i = 0; i < 5000; i++) {
        int32_t val = i;
        binwriter_write_entry(writer, 0, i, &val, sizeof(val));
    }

    /* Check that some data was written (buffer flushed) */
    uint64_t bytes = binwriter_get_bytes_written(writer);
    if (bytes < 64) /* Should have at least written header */
        TEST_FAIL("No bytes written");

    /* Check buffered bytes is less than total written */
    size_t buffered = binwriter_get_buffered_bytes(writer);
    if (buffered >= bytes)
        TEST_FAIL("Buffer not flushing");

    /* Manual flush */
    if (binwriter_flush(writer) != 0)
        TEST_FAIL("Manual flush failed");

    /* After flush, buffered should be 0 */
    if (binwriter_get_buffered_bytes(writer) != 0)
        TEST_FAIL("Buffer not empty after flush");

    binwriter_close(writer, NULL, 0);
    unlink(TEST_FILE);

    TEST_PASS();
    return 0;
}

/* Test statistics functions */
int test_statistics() {
    binary_writer_t* writer = binwriter_create(TEST_FILE);
    if (writer == NULL)
        TEST_FAIL("Failed to create writer");

    binwriter_write_header(writer, 1000000000ULL, 0, 1234567890, 0);

    /* Initially should be 0 entries */
    if (binwriter_get_entry_count(writer) != 0)
        TEST_FAIL("Initial entry count not 0");

    /* Write some entries */
    for (int i = 0; i < 10; i++) {
        binwriter_write_entry(writer, 0, i, NULL, 0);
    }

    /* Check count */
    if (binwriter_get_entry_count(writer) != 10)
        TEST_FAIL("Entry count not 10");

    /* Bytes written should be > 0 */
    if (binwriter_get_bytes_written(writer) == 0)
        TEST_FAIL("No bytes written");

    binwriter_close(writer, NULL, 0);
    unlink(TEST_FILE);

    TEST_PASS();
    return 0;
}

/* Main test runner */
int main() {
    int failures = 0;

    printf("CNanoLog Binary Writer Tests\n");
    printf("=============================\n\n");

    printf("Basic Tests:\n");
    failures += test_create_close();
    failures += test_write_header();

    printf("\nEntry Writing Tests:\n");
    failures += test_write_entries();
    failures += test_write_dictionary();

    printf("\nBuffering Tests:\n");
    failures += test_buffer_flush();

    printf("\nStatistics Tests:\n");
    failures += test_statistics();

    printf("\n=============================\n");
    if (failures == 0) {
        printf("All tests PASSED ✓\n");
        return 0;
    } else {
        printf("%d test(s) FAILED ✗\n", failures);
        return 1;
    }
}
