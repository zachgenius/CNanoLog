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
    LOG_INFO("Single-header test started");
    LOG_INFO("Test with one arg: %d", 42);
    LOG_INFO("Test with two args: %d, %s", 123, "hello");
    LOG_WARN("Warning message");
    LOG_ERROR("Error with code: %d", -1);
    LOG_DEBUG("Debug message");

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
