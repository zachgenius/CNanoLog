/*
 * Example demonstrating single-header usage of CNanoLog
 *
 * To build:
 *   1. Generate single-header: make single-header (or ./tools/generate_single_header.sh)
 *   2. Compile: gcc -std=c11 -pthread single_header_example.c -o single_header_example
 *   3. Run: ./single_header_example
 *   4. View logs: ./decompressor example_single.clog
 */

#define CNANOLOG_IMPLEMENTATION
#include "../cnanolog.h"

int main(void) {
    printf("CNanoLog Single-Header Example\n");
    printf("===============================\n\n");

    // Initialize logger
    if (cnanolog_init("example_single.clog") != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    printf("Logger initialized. Generating sample logs...\n");

    // Test various log levels and argument counts
    log_info("Application started");
    log_info1("Processing %d items", 100);
    log_info2("Server running on %s:%d", "localhost", 8080);
    log_warn1("Memory usage: %d MB", 512);
    log_error1("Failed to connect to database (error: %d)", -1);
    log_debug("Debug information");

    // Test with different data types
    log_info3("Position: x=%d, y=%d, z=%d", 10, 20, 30);
    log_info2("Temperature: %.2f°C, Humidity: %d%%", 23.5, 65);

    // Get statistics
    cnanolog_stats_t stats;
    cnanolog_get_stats(&stats);

    printf("\nLogging Statistics:\n");
    printf("  Logs written:   %llu\n", (unsigned long long)stats.total_logs_written);
    printf("  Bytes written:  %llu\n", (unsigned long long)stats.total_bytes_written);
    printf("  Dropped logs:   %llu\n", (unsigned long long)stats.dropped_logs);
    printf("  Compression:    %.2fx\n", stats.compression_ratio_x100 / 100.0);

    // Shutdown
    cnanolog_shutdown();

    printf("\nLogs written to: example_single.clog\n");
    printf("To view logs: ./decompressor example_single.clog\n");

    return 0;
}
