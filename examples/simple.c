#include <cnanolog.h>
#include <stdio.h> // For printf

int main() {
    if (cnanolog_init("test_log.txt") != 0) {
        printf("Failed to initialize logger.\n");
        return 1;
    }

    LOG_INFO("Application starting up.");

    for (int i = 0; i < 10; ++i) {
        LOG_DEBUG("Loop iteration %d", i);
    }

    LOG_WARN("This is a warning message.");
    LOG_ERROR("This is an error message. Something went wrong!");

    LOG_INFO("Application shutting down.");

    cnanolog_shutdown();

    printf("Log messages written to test_log.txt\n");

    return 0;
}