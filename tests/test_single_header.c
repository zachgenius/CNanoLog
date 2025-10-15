/*
 * Test for single-header version of CNanoLog
 * Verifies that the single-header file compiles and works correctly
 */

#define CNANOLOG_IMPLEMENTATION
#include "../cnanolog.h"

#include <stdio.h>
#include <unistd.h>

int main(void) {
    printf("Testing single-header CNanoLog...\n");

    // Initialize logger
    if (cnanolog_init("test_single_header.clog") != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    // Test basic logging
    log_info("Single-header test started");
    log_info1("Test with one arg: %d", 42);
    log_info2("Test with two args: %d, %s", 123, "hello");
    log_warn("Warning message");
    log_error1("Error with code: %d", -1);
    log_debug("Debug message");

    // Test statistics
    cnanolog_stats_t stats;
    cnanolog_get_stats(&stats);
    printf("Logs written: %llu\n", (unsigned long long)stats.total_logs_written);
    printf("Bytes written: %llu\n", (unsigned long long)stats.total_bytes_written);
    printf("Dropped logs: %llu\n", (unsigned long long)stats.dropped_logs);

    // Shutdown
    cnanolog_shutdown();

    printf("Single-header test completed successfully!\n");
    return 0;
}
