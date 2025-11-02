#include <cnanolog.h>
#include <stdio.h>

int main(void) {
    printf("Testing CNanoLog Conan package\n");

    /* Initialize logger */
    if (cnanolog_init("conan_test.clog") != 0) {
        fprintf(stderr, "ERROR: Failed to initialize logger\n");
        return 1;
    }

    /* Log some messages */
    log_info("Conan package test started");
    LOG_Uinfo("Test with integer: %d", 42);
    LOG_Uinfo("Test with multiple args: %d and %d", 10, 20);
    log_warn("This is a warning");
    log_error("This is an error (test only)");

    /* Get statistics */
    cnanolog_stats_t stats;
    cnanolog_get_stats(&stats);

    printf("Successfully logged %llu messages\n",
           (unsigned long long)stats.total_logs_written);

    /* Shutdown */
    cnanolog_shutdown();

    printf("SUCCESS: CNanoLog Conan package is working!\n");
    return 0;
}
