#include <cnanolog.h>

void cnanolog_log(log_level_t level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    switch (level) {
        case LOG_LEVEL_INFO:
            fprintf(stdout, "[INFO] ");
            break;
        case LOG_LEVEL_WARN:
            fprintf(stdout, "[WARN] ");
            break;
        case LOG_LEVEL_ERROR:
            fprintf(stderr, "[ERROR] ");
            break;
    }

    vfprintf(level == LOG_LEVEL_ERROR ? stderr : stdout, fmt, args);
    fprintf(level == LOG_LEVEL_ERROR ? stderr : stdout, "\n");

    va_end(args);
}
