#include <cnanolog.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define ENTRY_COUNT 50000
#define OUTPUT_PATH "test_shutdown_drain.log"

int main(void) {
    unlink(OUTPUT_PATH);

    cnanolog_rotation_config_t config = {
        .policy = CNANOLOG_ROTATE_NONE,
        .base_path = OUTPUT_PATH,
        .format = CNANOLOG_OUTPUT_TEXT,
        .text_pattern = "%m",
    };
    if (cnanolog_init_ex(&config) != 0) {
        return 1;
    }

    for (int entry_index = 0; entry_index < ENTRY_COUNT; entry_index++) {
        LOG_INFO("entry=%d", entry_index);
    }
    cnanolog_shutdown();

    FILE* output = fopen(OUTPUT_PATH, "r");
    if (output == NULL) {
        return 1;
    }

    char line[128];
    int line_count = 0;
    while (fgets(line, sizeof(line), output) != NULL) {
        if (strstr(line, "[UNKNOWN_LOG_ID_") != NULL) {
            fclose(output);
            unlink(OUTPUT_PATH);
            return 1;
        }
        line_count++;
    }

    fclose(output);
    unlink(OUTPUT_PATH);
    return line_count == ENTRY_COUNT ? 0 : 1;
}
