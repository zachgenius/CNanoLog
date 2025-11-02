/*
 * Basic Usage Example
 *
 * Demonstrates:
 * - Logger initialization and shutdown
 * - Different log levels (info, warn, error, debug)
 * - Logging with different argument types (int, string, etc.)
 * - Variadic macros for different argument counts
 */

#include <cnanolog.h>
#include <stdio.h>

int main(void) {
    printf("=== CNanoLog Basic Usage Example ===\n\n");

    /* Initialize the logger */
    if (cnanolog_init("basic_example.clog") != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    printf("Logger initialized successfully\n");
    printf("Writing logs to: basic_example.clog (binary format)\n");
    printf("Use ../tools/decompressor to view logs\n\n");

    /* Log at different levels */
    LOG_INFO("Application started");
    LOG_INFO("This is an informational message");

    LOG_WARN("This is a warning message");
    LOG_ERROR("This is an error message");
    LOG_DEBUG("This is a debug message");

    /* Logging with arguments */
    printf("Logging with different argument types...\n");

    // Single integer
    int status_code = 200;
    LOG_INFO("HTTP status code: %d", status_code);

    // Multiple integers
    int x = 10, y = 20, z = 30;
    LOG_INFO("Position: x=%d y=%d z=%d", x, y, z);

    // String argument
    const char* username = "alice";
    LOG_INFO("User logged in: %s", username);

    // Mixed arguments
    const char* operation = "database_query";
    int duration_ms = 42;
    LOG_INFO("Operation '%s' completed in %d ms", operation, duration_ms);

    // Numeric values
    LOG_INFO("Processing item %d", 12345);
    LOG_INFO("Memory usage: %d KB / %d KB", 512, 1024);

    // Error scenarios
    int error_code = 500;
    const char* error_msg = "Internal server error";
    LOG_ERROR("Error %d: %s", error_code, error_msg);

    // Warning with details
    int retry_count = 3;
    LOG_WARN("Connection failed, retry attempt %d", retry_count);

    /* Performance test - log many messages */
    printf("\nPerformance test: Logging 10,000 messages...\n");

    for (int i = 0; i < 10000; i++) {
        LOG_INFO("Message %d: value=%d", i, i * 2);
    }

    printf("Performance test complete\n");

    /* Get statistics before shutdown */
    cnanolog_stats_t stats;
    cnanolog_get_stats(&stats);

    printf("\nStatistics:\n");
    printf("  Total logs written:  %llu\n", (unsigned long long)stats.total_logs_written);
    printf("  Total bytes written: %llu bytes\n", (unsigned long long)stats.total_bytes_written);
    printf("  Compression ratio:   %.2fx\n", stats.compression_ratio_x100 / 100.0);
    printf("  Dropped logs:        %llu\n", (unsigned long long)stats.dropped_logs);

    /* Shutdown the logger */
    cnanolog_shutdown();
    printf("\nLogger shut down successfully\n");

    printf("\n=== Example Complete ===\n");
    printf("To view logs, run:\n");
    printf("  ../tools/decompressor basic_example.clog\n\n");

    return 0;
}
