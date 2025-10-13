#ifndef CNANOLOG_H
#define CNANOLOG_H

#include <stdio.h>
#include <stdarg.h>

typedef enum {
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} log_level_t;

void cnanolog_log(log_level_t level, const char *fmt, ...);

#define log_info(...) cnanolog_log(LOG_LEVEL_INFO, __VA_ARGS__)
#define log_warn(...) cnanolog_log(LOG_LEVEL_WARN, __VA_ARGS__)
#define log_error(...) cnanolog_log(LOG_LEVEL_ERROR, __VA_ARGS__)

#endif //CNANOLOG_H