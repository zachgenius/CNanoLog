#pragma once

#include <stdarg.h> // For va_list

// Initializes the logging system and starts the background writer thread.
// Must be called once at application startup.
// Returns 0 on success, -1 on failure.
int cnanolog_init(const char* log_file_path);

// Shuts down the logger, flushes all remaining messages, and joins the background thread.
void cnanolog_shutdown();

typedef enum {
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_DEBUG
} cnanolog_level_t;

// Internal logging function. Use the macros below.
void _cnanolog_log_internal(cnanolog_level_t level, const char* file, int line, const char* format, ...);

// User-facing logging macros
#define log_info(...)  _cnanolog_log_internal(LOG_LEVEL_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...)  _cnanolog_log_internal(LOG_LEVEL_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) _cnanolog_log_internal(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...) _cnanolog_log_internal(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
