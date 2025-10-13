/*
 * Test program for CNanoLog statistics API
 */

#include <cnanolog.h>
#include <stdio.h>
#include <time.h>

int main(void) {
    printf("CNanoLog Statistics API Test\n");
    printf("==============================\n\n");

    /* Initialize logger */
    if (cnanolog_init("test_stats.clog") != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    /* Preallocate buffer (Phase 6.2 feature) */
    cnanolog_preallocate();

    printf("Writing test log entries...\n");

    /* Write various log entries */
    log_info("Test message 1");
    log_info1("Test with integer: %d", 42);
    log_info2("Test with two integers: %d %d", 10, 20);
    log_info1("Test with string: %s", "Hello");
    log_warn("Warning message");
    log_error1("Error message with code: %d", 500);
    log_debug2("Debug message: x=%d, y=%d", 100, 200);

    /* Write more entries to see compression ratio */
    for (int i = 0; i < 100; i++) {
        log_info1("Loop iteration %d", i);
    }

    /* Give background thread time to process */
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 100000000;  /* 100ms */
    nanosleep(&ts, NULL);

    printf("\n");

    /* Get statistics BEFORE shutdown */
    cnanolog_stats_t stats;
    cnanolog_get_stats(&stats);

    /* Shutdown logger */
    cnanolog_shutdown();

    printf("Statistics:\n");
    printf("-----------\n");
    printf("Total logs written:     %llu\n", (unsigned long long)stats.total_logs_written);
    printf("Dropped logs:           %llu\n", (unsigned long long)stats.dropped_logs);
    printf("Total bytes written:    %llu bytes\n", (unsigned long long)stats.total_bytes_written);
    printf("Compression ratio:      %.2fx\n", stats.compression_ratio_x100 / 100.0);
    printf("Staging buffers active: %llu\n", (unsigned long long)stats.staging_buffers_active);
    printf("Background wakeups:     %llu\n", (unsigned long long)stats.background_wakeups);

    printf("\n==============================\n");
    printf("✓ Statistics test completed!\n");

    return 0;
}
