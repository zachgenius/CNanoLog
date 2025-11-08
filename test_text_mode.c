/* Test program for text mode logging */

#include "include/cnanolog.h"
#include <stdio.h>
#include <unistd.h>

int main(void) {
    printf("Testing CNanoLog text mode...\n");

    /* Initialize with text mode */
    cnanolog_rotation_config_t config = {
        .policy = CNANOLOG_ROTATE_NONE,
        .base_path = "test_text_mode.log",
        .format = CNANOLOG_OUTPUT_TEXT
    };

    if (cnanolog_init_ex(&config) != 0) {
        fprintf(stderr, "Failed to initialize text mode\n");
        return 1;
    }

    printf("Logging some messages...\n");

    /* Log various messages */
    LOG_INFO("Starting text mode test");
    LOG_INFO("Integer test: %d", 42);
    LOG_INFO("String test: %s", "Hello, text mode!");
    LOG_INFO("Multiple args: %d %s %d", 1, "two", 3);
    LOG_WARN("This is a warning message");
    LOG_ERROR("This is an error message");
    LOG_DEBUG("Debug info: value=%d", 12345);

    /* Give background thread time to process */
    usleep(100000);  /* 100ms */

    printf("Shutting down...\n");
    cnanolog_shutdown();

    printf("\nText log file contents:\n");
    printf("================================================================================\n");
    FILE* f = fopen("test_text_mode.log", "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            printf("%s", line);
        }
        fclose(f);
    }
    printf("================================================================================\n");

    printf("\nTest completed successfully!\n");
    return 0;
}
