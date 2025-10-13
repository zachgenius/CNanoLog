/*
 * End-to-End Test
 * Tests the complete pipeline: write binary log -> decompress -> verify output
 */

#include "../src/binary_writer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define TEST_BINARY_FILE "test_e2e.clog"
#define TEST_TEXT_FILE "test_e2e.txt"

int main() {
    printf("CNanoLog End-to-End Test\n");
    printf("=========================\n\n");

    /* ====================================================================
     * Phase 1: Write Binary Log
     * ==================================================================== */

    printf("Phase 1: Writing binary log...\n");

    binary_writer_t* writer = binwriter_create(TEST_BINARY_FILE);
    if (writer == NULL) {
        fprintf(stderr, "FAIL: Cannot create writer\n");
        return 1;
    }

    /* Write header with timing info */
    uint64_t freq = 1000000000ULL;  /* 1 GHz for easy calculation */
    uint64_t start_ticks = 0;
    time_t start_sec = 1700000000;  /* Nov 2023 */

    if (binwriter_write_header(writer, freq, start_ticks, start_sec, 0) != 0) {
        fprintf(stderr, "FAIL: Cannot write header\n");
        return 1;
    }

    /* Write entry 1: No arguments */
    if (binwriter_write_entry(writer, 0, 0, NULL, 0) != 0) {
        fprintf(stderr, "FAIL: Cannot write entry 1\n");
        return 1;
    }

    /* Write entry 2: One integer */
    int32_t value1 = 42;
    if (binwriter_write_entry(writer, 1, 1000000000, &value1, sizeof(value1)) != 0) {
        fprintf(stderr, "FAIL: Cannot write entry 2\n");
        return 1;
    }

    /* Write entry 3: Two integers */
    struct {
        int32_t a;
        int32_t b;
    } values2 = {100, 200};
    if (binwriter_write_entry(writer, 2, 2000000000, &values2, sizeof(values2)) != 0) {
        fprintf(stderr, "FAIL: Cannot write entry 3\n");
        return 1;
    }

    /* Write entry 4: String */
    const char* str = "Hello";
    uint32_t str_len = strlen(str);
    char str_buffer[32];
    memcpy(str_buffer, &str_len, sizeof(str_len));
    memcpy(str_buffer + sizeof(str_len), str, str_len);
    if (binwriter_write_entry(writer, 3, 3000000000, str_buffer,
                               sizeof(str_len) + str_len) != 0) {
        fprintf(stderr, "FAIL: Cannot write entry 4\n");
        return 1;
    }

    /* Prepare dictionary */
    log_site_info_t sites[4];

    sites[0].log_id = 0;
    sites[0].log_level = LOG_LEVEL_INFO;
    sites[0].filename = "test.c";
    sites[0].format = "Application started";
    sites[0].line_number = 10;
    sites[0].num_args = 0;

    sites[1].log_id = 1;
    sites[1].log_level = LOG_LEVEL_INFO;
    sites[1].filename = "test.c";
    sites[1].format = "Processing item %d";
    sites[1].line_number = 20;
    sites[1].num_args = 1;
    sites[1].arg_types[0] = ARG_TYPE_INT32;

    sites[2].log_id = 2;
    sites[2].log_level = LOG_LEVEL_WARN;
    sites[2].filename = "test.c";
    sites[2].format = "Values: %d and %d";
    sites[2].line_number = 30;
    sites[2].num_args = 2;
    sites[2].arg_types[0] = ARG_TYPE_INT32;
    sites[2].arg_types[1] = ARG_TYPE_INT32;

    sites[3].log_id = 3;
    sites[3].log_level = LOG_LEVEL_ERROR;
    sites[3].filename = "test.c";
    sites[3].format = "Error: %s";
    sites[3].line_number = 40;
    sites[3].num_args = 1;
    sites[3].arg_types[0] = ARG_TYPE_STRING;

    /* Close with dictionary */
    if (binwriter_close(writer, sites, 4) != 0) {
        fprintf(stderr, "FAIL: Cannot close writer\n");
        return 1;
    }

    printf("  ✓ Wrote 4 log entries to %s\n", TEST_BINARY_FILE);

    /* ====================================================================
     * Phase 2: Decompress Binary Log
     * ==================================================================== */

    printf("\nPhase 2: Decompressing binary log...\n");

    /* Run decompressor (assume it's in same directory when tests run from build/) */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "./decompressor %s %s 2>&1",
             TEST_BINARY_FILE, TEST_TEXT_FILE);
    int result = system(cmd);
    if (result != 0) {
        fprintf(stderr, "FAIL: Decompressor failed (exit code %d)\n", result);
        return 1;
    }

    printf("  ✓ Decompressed to %s\n", TEST_TEXT_FILE);

    /* ====================================================================
     * Phase 3: Verify Output
     * ==================================================================== */

    printf("\nPhase 3: Verifying decompressed output...\n");

    FILE* fp = fopen(TEST_TEXT_FILE, "r");
    if (fp == NULL) {
        fprintf(stderr, "FAIL: Cannot open decompressed file\n");
        return 1;
    }

    char line[512];
    int line_count = 0;
    (void)line_count;  /* Used for tracking but not checked */

    /* Line 1: Application started */
    if (fgets(line, sizeof(line), fp) == NULL) {
        fprintf(stderr, "FAIL: Cannot read line 1\n");
        fclose(fp);
        return 1;
    }
    if (strstr(line, "[INFO]") == NULL || strstr(line, "Application started") == NULL ||
        strstr(line, "test.c:10") == NULL) {
        fprintf(stderr, "FAIL: Line 1 incorrect: %s", line);
        fclose(fp);
        return 1;
    }
    printf("  ✓ Line 1: %s", line);
    line_count++;

    /* Line 2: Processing item 42 */
    if (fgets(line, sizeof(line), fp) == NULL) {
        fprintf(stderr, "FAIL: Cannot read line 2\n");
        fclose(fp);
        return 1;
    }
    if (strstr(line, "[INFO]") == NULL || strstr(line, "Processing item 42") == NULL ||
        strstr(line, "test.c:20") == NULL) {
        fprintf(stderr, "FAIL: Line 2 incorrect: %s", line);
        fclose(fp);
        return 1;
    }
    printf("  ✓ Line 2: %s", line);
    line_count++;

    /* Line 3: Values: 100 and 200 */
    if (fgets(line, sizeof(line), fp) == NULL) {
        fprintf(stderr, "FAIL: Cannot read line 3\n");
        fclose(fp);
        return 1;
    }
    if (strstr(line, "[WARN]") == NULL || strstr(line, "Values: 100 and 200") == NULL ||
        strstr(line, "test.c:30") == NULL) {
        fprintf(stderr, "FAIL: Line 3 incorrect: %s", line);
        fclose(fp);
        return 1;
    }
    printf("  ✓ Line 3: %s", line);
    line_count++;

    /* Line 4: Error: Hello */
    if (fgets(line, sizeof(line), fp) == NULL) {
        fprintf(stderr, "FAIL: Cannot read line 4\n");
        fclose(fp);
        return 1;
    }
    if (strstr(line, "[ERROR]") == NULL || strstr(line, "Error: Hello") == NULL ||
        strstr(line, "test.c:40") == NULL) {
        fprintf(stderr, "FAIL: Line 4 incorrect: %s", line);
        fclose(fp);
        return 1;
    }
    printf("  ✓ Line 4: %s", line);
    line_count++;

    fclose(fp);

    /* ====================================================================
     * Cleanup
     * ==================================================================== */

    printf("\nCleaning up...\n");
    unlink(TEST_BINARY_FILE);
    unlink(TEST_TEXT_FILE);

    /* ====================================================================
     * Summary
     * ==================================================================== */

    printf("\n=========================\n");
    printf("✓ All tests PASSED\n");
    printf("  - Binary log created with 4 entries\n");
    printf("  - Decompressed successfully\n");
    printf("  - Output verified correct\n");
    printf("\n");

    return 0;
}
