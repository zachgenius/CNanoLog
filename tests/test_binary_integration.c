/* Test Binary Logging Integration */

#include "../include/cnanolog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main() {
    printf("Binary Logging Integration Test\n");
    printf("=================================\n\n");

    /* Initialize binary logger */
    printf("1. Initializing logger...\n");
    if (cnanolog_init("test_integration.clog") != 0) {
        fprintf(stderr, "FAIL: cnanolog_init failed\n");
        return 1;
    }
    printf("   ✓ Logger initialized\n\n");

    /* Test various log levels and argument types */
    printf("2. Writing log entries...\n");

    log_info("Application started");
    log_info1("Processing count: %d", 42);
    log_warn1("Warning: threshold exceeded: %d", 100);
    log_error2("Error code: %d, message: %s", 500, "Internal error");

    int x = 10, y = 20;
    log_debug3("Debug: x=%d, y=%d, sum=%d", x, y, x + y);

    const char* user = "Alice";
    log_info1("User %s logged in", user);

    double pi = 3.14159;
    log_info1("Pi value: %f", pi);

    unsigned int count = 1000U;
    log_info1("Count: %u", count);

    printf("   ✓ Wrote 8 log entries\n\n");

    /* Give background thread time to flush */
    printf("3. Flushing logs...\n");
    sleep(1);
    printf("   ✓ Logs flushed\n\n");

    /* Shutdown */
    printf("4. Shutting down logger...\n");
    cnanolog_shutdown();
    printf("   ✓ Logger shut down\n\n");

    /* Decompress and verify */
    printf("5. Decompressing log...\n");
    int ret = system("../tools/decompressor test_integration.clog test_integration.txt 2>&1");
    if (ret != 0) {
        fprintf(stderr, "FAIL: Decompressor failed (exit code %d)\n", ret);
        return 1;
    }
    printf("   ✓ Log decompressed\n\n");

    /* Show decompressed output */
    printf("6. Decompressed output:\n");
    printf("-----------------------------------\n");
    system("cat test_integration.txt");
    printf("-----------------------------------\n\n");

    /* Basic verification */
    printf("7. Verifying output...\n");
    FILE* fp = fopen("test_integration.txt", "r");
    if (!fp) {
        fprintf(stderr, "FAIL: Cannot open decompressed file\n");
        return 1;
    }

    char line[512];
    int line_count = 0;
    int has_started = 0;
    int has_processing = 0;
    int has_warning = 0;
    int has_error = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        line_count++;
        if (strstr(line, "Application started")) has_started = 1;
        if (strstr(line, "Processing count: 42")) has_processing = 1;
        if (strstr(line, "threshold exceeded: 100")) has_warning = 1;
        if (strstr(line, "Error code: 500")) has_error = 1;
    }
    fclose(fp);

    if (!has_started || !has_processing || !has_warning || !has_error) {
        fprintf(stderr, "FAIL: Missing expected log messages\n");
        return 1;
    }

    if (line_count < 8) {
        fprintf(stderr, "FAIL: Expected at least 8 log lines, got %d\n", line_count);
        return 1;
    }

    printf("   ✓ Found all expected messages\n");
    printf("   ✓ Total lines: %d\n\n", line_count);

    /* Cleanup */
    remove("test_integration.clog");
    remove("test_integration.txt");

    printf("=================================\n");
    printf("✓ All tests PASSED\n");
    printf("Binary logging system is working!\n");

    return 0;
}
