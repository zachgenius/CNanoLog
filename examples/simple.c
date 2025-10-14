#include <cnanolog.h>
#include <stdio.h> // For printf

int main() {
    if (cnanolog_init("test_log.txt") != 0) {
        printf("Failed to initialize logger.\n");
        return 1;
    }

    log_info("Application starting up.");

    for (int i = 0; i < 10; ++i) {
        log_debug1("Loop iteration %d", i);
    }

    log_warn("This is a warning message.");
    log_error("This is an error message. Something went wrong!");

    log_info("Application shutting down.");

    cnanolog_shutdown();

    printf("Log messages written to test_log.txt\n");

    return 0;
}