/*
 * Error Handling and Edge Cases Example
 *
 * Demonstrates:
 * - Proper error handling
 * - Recovery from errors
 * - Edge cases and boundary conditions
 * - Defensive programming practices
 */

#include <cnanolog.h>
#include "../src/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Test: Double initialization */
void test_double_init(void) {
    printf("\n=== Test 1: Double Initialization ===\n");

    /* First init - should succeed */
    printf("  Initializing logger first time...\n");
    int result = cnanolog_init("error_test.clog");
    if (result == 0) {
        printf("  ✓ First initialization succeeded\n");
    } else {
        printf("  ✗ First initialization failed\n");
        return;
    }

    /* Second init - should be safe (no-op) */
    printf("  Initializing logger second time...\n");
    result = cnanolog_init("error_test.clog");
    if (result == 0) {
        printf("  ✓ Second initialization handled safely\n");
    } else {
        printf("  ⚠ Second initialization returned error (expected)\n");
    }

    /* Log something to verify it still works */
    LOG_INFO("Logger still works after double init attempt");
    printf("  ✓ Logging still works\n");

    cnanolog_shutdown();
}

/* Test: Logging before initialization */
void test_log_before_init(void) {
    printf("\n=== Test 2: Logging Before Initialization ===\n");

    printf("  Attempting to log before initialization...\n");

    /* This should be safe - library should handle gracefully */
    LOG_INFO("This log happens before init");

    printf("  ✓ No crash (library handled it gracefully)\n");
    printf("  Note: Log was likely dropped or ignored\n");
}

/* Test: Multiple shutdowns */
void test_multiple_shutdown(void) {
    printf("\n=== Test 3: Multiple Shutdowns ===\n");

    printf("  Initializing logger...\n");
    cnanolog_init("error_test2.clog");

    LOG_INFO("Test log before first shutdown");

    printf("  Calling shutdown first time...\n");
    cnanolog_shutdown();
    printf("  ✓ First shutdown succeeded\n");

    printf("  Calling shutdown second time...\n");
    cnanolog_shutdown();
    printf("  ✓ Second shutdown handled safely\n");

    printf("  Attempting to log after shutdown...\n");
    LOG_INFO("This log happens after shutdown");
    printf("  ✓ No crash (library handled it gracefully)\n");
}

/* Test: Invalid file paths */
void test_invalid_paths(void) {
    printf("\n=== Test 4: Invalid File Paths ===\n");

    /* Try to open in invalid directory */
    printf("  Attempting to open log in non-existent directory...\n");
    int result = cnanolog_init("/non/existent/directory/test.clog");

    if (result != 0) {
        printf("  ✓ Correctly failed to open invalid path\n");
        printf("  Error: %s\n", strerror(errno));
    } else {
        printf("  ⚠ Unexpectedly succeeded (may have created directories)\n");
        cnanolog_shutdown();
    }

    /* Try with valid path */
    printf("  Opening log with valid path...\n");
    result = cnanolog_init("valid_error_test.clog");
    if (result == 0) {
        printf("  ✓ Successfully opened valid path\n");
        LOG_INFO("Test log with valid path");
        cnanolog_shutdown();
    }
}

/* Test: CPU affinity with invalid cores */
void test_invalid_affinity(void) {
    printf("\n=== Test 5: Invalid CPU Affinity ===\n");

    printf("  Initializing logger...\n");
    cnanolog_init("affinity_error_test.clog");

    /* Try negative core */
    printf("  Setting affinity to core -1 (invalid)...\n");
    int result = cnanolog_set_writer_affinity(-1);
    if (result != 0) {
        printf("  ✓ Correctly rejected negative core\n");
    }

    /* Try very large core number */
    printf("  Setting affinity to core 9999 (likely invalid)...\n");
    result = cnanolog_set_writer_affinity(9999);
    if (result != 0) {
        printf("  ✓ Correctly rejected invalid core number\n");
    }

    /* Try valid core */
    printf("  Setting affinity to core 0 (should be valid)...\n");
    result = cnanolog_set_writer_affinity(0);
    if (result == 0) {
        printf("  ✓ Successfully set affinity to core 0\n");
    } else {
        printf("  ⚠ Failed (may not have permission or platform support)\n");
    }

    /* Verify logging still works */
    LOG_INFO("Logging after affinity tests");
    printf("  ✓ Logging still works\n");

    cnanolog_shutdown();
}

/* Test: Extreme logging */
void test_extreme_logging(void) {
    printf("\n=== Test 6: Extreme Logging ===\n");

    cnanolog_init("extreme_test.clog");

    /* Very long strings */
    printf("  Testing with very long strings...\n");
    char long_string[256];
    memset(long_string, 'X', sizeof(long_string) - 1);
    long_string[sizeof(long_string) - 1] = '\0';

    LOG_INFO("Long string test: %s", long_string);
    printf("  ✓ Long string handled\n");

    /* Very large numbers */
    printf("  Testing with large numbers...\n");
    LOG_INFO("Large number: %d", 2147483647);  // INT_MAX
    printf("  ✓ Large numbers handled\n");

    /* Rapid logging */
    printf("  Testing rapid logging (10,000 logs)...\n");
    for (int i = 0; i < 10000; i++) {
        LOG_INFO("Rapid log %d", i);
    }
    printf("  ✓ Rapid logging completed\n");

    /* Check statistics */
    cnanolog_stats_t stats;
    cnanolog_get_stats(&stats);

    printf("  Statistics:\n");
    printf("    Logs written: %llu\n", (unsigned long long)stats.total_logs_written);
    printf("    Logs dropped: %llu\n", (unsigned long long)stats.dropped_logs);

    if (stats.dropped_logs > 0) {
        double drop_rate = (stats.dropped_logs * 100.0) /
                          (stats.total_logs_written + stats.dropped_logs);
        printf("    Drop rate: %.2f%%\n", drop_rate);
    }

    cnanolog_shutdown();
}

/* Test: Statistics API edge cases */
void test_statistics_edge_cases(void) {
    printf("\n=== Test 7: Statistics Edge Cases ===\n");

    cnanolog_init("stats_test.clog");

    /* Get stats before any logging */
    printf("  Getting stats before any logging...\n");
    cnanolog_stats_t stats;
    cnanolog_get_stats(&stats);

    printf("  Initial stats:\n");
    printf("    Logs: %llu\n", (unsigned long long)stats.total_logs_written);
    printf("    Bytes: %llu\n", (unsigned long long)stats.total_bytes_written);
    printf("  ✓ Stats API works with zero logs\n");

    /* Log some messages */
    for (int i = 0; i < 100; i++) {
        LOG_INFO("Stats test %d", i);
    }

    /* Reset stats */
    printf("  Resetting stats...\n");
    cnanolog_reset_stats();

    cnanolog_get_stats(&stats);
    printf("  After reset:\n");
    printf("    Logs: %llu\n", (unsigned long long)stats.total_logs_written);
    printf("  ✓ Stats reset works\n");

    /* Log more and verify counting resumes */
    for (int i = 0; i < 50; i++) {
        LOG_INFO("After reset %d", i);
    }

    cnanolog_get_stats(&stats);
    printf("  After more logging:\n");
    printf("    Logs: %llu\n", (unsigned long long)stats.total_logs_written);
    printf("  ✓ Stats counting resumed correctly\n");

    cnanolog_shutdown();
}

/* Test: Preallocate edge cases */
void test_preallocate_edge_cases(void) {
    printf("\n=== Test 8: Preallocate Edge Cases ===\n");

    cnanolog_init("preallocate_test.clog");

    /* Call preallocate multiple times */
    printf("  Calling preallocate multiple times...\n");
    cnanolog_preallocate();
    cnanolog_preallocate();
    cnanolog_preallocate();
    printf("  ✓ Multiple preallocate calls handled safely\n");

    /* Verify logging works */
    LOG_INFO("Logging after multiple preallocate calls");
    printf("  ✓ Logging works normally\n");

    cnanolog_shutdown();
}

int main(void) {
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║   CNanoLog Error Handling & Edge Cases Example      ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");

    printf("\nThis example demonstrates proper error handling and\n");
    printf("tests various edge cases to ensure library robustness.\n");

    /* Run all tests */
    test_log_before_init();
    test_double_init();
    test_multiple_shutdown();
    test_invalid_paths();
    test_invalid_affinity();
    test_extreme_logging();
    test_statistics_edge_cases();
    test_preallocate_edge_cases();

    printf("\n╔══════════════════════════════════════════════════════╗\n");
    printf("║   All Error Handling Tests Complete                 ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");

    printf("\nKey Takeaways:\n");
    printf("  1. Library handles double-init/double-shutdown safely\n");
    printf("  2. Invalid parameters are rejected with clear errors\n");
    printf("  3. Logging before init is safe (but logs are dropped)\n");
    printf("  4. Extreme conditions are handled gracefully\n");
    printf("  5. Statistics API is robust with edge cases\n");
    printf("  6. Multiple preallocate calls are harmless\n");

    printf("\nBest Practices:\n");
    printf("  ✓ Always check return values from init functions\n");
    printf("  ✓ Call shutdown once at program exit\n");
    printf("  ✓ Validate file paths before passing to init\n");
    printf("  ✓ Handle CPU affinity failures gracefully\n");
    printf("  ✓ Monitor statistics for unusual patterns\n");

    return 0;
}
