/* Test for Per-Log Format Patterns
 *
 * Demonstrates and tests the LOG_*_FMT macros that allow
 * individual log calls to override the global text pattern.
 */

#include "../include/cnanolog.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(void) {
    printf("Testing per-log format patterns\n");
    printf("========================================\n\n");

    /* Initialize with text mode and a default pattern */
    cnanolog_rotation_config_t config = {
        .policy = CNANOLOG_ROTATE_NONE,
        .base_path = "test_per_log_pattern.log",
        .format = CNANOLOG_OUTPUT_TEXT,
        .text_pattern = "[%t] [%l] %m"  /* Default: timestamp, level, message */
    };

    if (cnanolog_init_ex(&config) != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    printf("1. Testing default global pattern...\n");
    LOG_INFO("This uses the global pattern");
    LOG_WARN("Warning with global pattern: %d", 42);
    LOG_ERROR("Error with global pattern: %s", "test error");

    printf("2. Testing per-log custom patterns...\n");

    /* Compact pattern (just time and message) */
    LOG_INFO_FMT("%T %m", "Compact format: port %d", 8080);

    /* JSON format for structured logging */
    LOG_WARN_FMT("{\"time\":\"%t\",\"level\":\"%l\",\"msg\":\"%m\"}",
                 "JSON format: memory usage %d%%", 85);

    /* Logfmt style */
    LOG_ERROR_FMT("time=\"%t\" level=%l file=%f:%n msg=\"%m\"",
                  "Logfmt style: connection failed");

    /* Minimal format (just message) */
    LOG_DEBUG_FMT("%m", "Minimal format: debug value = %d", 123);

    /* Syslog style */
    LOG_INFO_FMT("%d %D myapp[%L]: %m",
                 "Syslog style: processing request %d", 456);

    /* Verbose format with all fields */
    LOG_WARN_FMT("[%t] [%l] [%F:%n] %m",
                 "Verbose format with full file path");

    printf("3. Testing mixed usage (global and custom patterns)...\n");
    LOG_INFO("Back to global pattern");
    LOG_WARN_FMT("%T [%L] %m", "Custom pattern again");
    LOG_ERROR("Global pattern again: code %d", 500);

    printf("4. Testing with different argument types...\n");
    LOG_INFO_FMT("%T %m", "String arg: %s, int arg: %d, float arg: %.2f",
                 "test", 42, 3.14);

    LOG_WARN_FMT("{\"level\":\"%l\",\"msg\":\"%m\"}",
                 "Multiple args: %d %d %d", 1, 2, 3);

    /* Give background thread time to process all logs */
    printf("\nWaiting for logs to be written...\n");
    usleep(500000);  /* 500ms should be enough */

    /* Flush and shutdown */
    cnanolog_shutdown();

    printf("\n========================================\n");
    printf("Test completed!\n");
    printf("Check 'test_per_log_pattern.log' to verify output formats:\n");
    printf("  - Lines with global pattern: [timestamp] [level] message\n");
    printf("  - Lines with custom patterns: various formats\n\n");

    /* Display the log file contents */
    printf("Log file contents:\n");
    printf("------------------\n");
    FILE* log_file = fopen("test_per_log_pattern.log", "r");
    if (log_file != NULL) {
        char line[512];
        while (fgets(line, sizeof(line), log_file) != NULL) {
            printf("%s", line);
        }
        fclose(log_file);
    } else {
        printf("Could not open log file for display\n");
    }

    return 0;
}
