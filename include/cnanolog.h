#pragma once

#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Initialization & Shutdown
 * ============================================================================ */

/**
 * Initialize the logging system with binary format.
 * Must be called once at application startup.
 *
 * @param log_file_path Path to binary log file (*.clog)
 * @return 0 on success, -1 on failure
 */
int cnanolog_init(const char* log_file_path);

/**
 * Shut down the logger, flush all messages, and write dictionary.
 */
void cnanolog_shutdown(void);

/* ============================================================================
 * Log Levels
 * ============================================================================ */

typedef enum {
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_DEBUG
} cnanolog_level_t;

/* ============================================================================
 * Internal API (do not call directly)
 * ============================================================================ */

/**
 * Register a log site and return its unique ID.
 * Called automatically by log macros on first use.
 */
uint32_t _cnanolog_register_site(cnanolog_level_t level,
                                  const char* filename,
                                  uint32_t line_number,
                                  const char* format,
                                  uint8_t num_args,
                                  const uint8_t* arg_types);

/**
 * Write a binary log entry.
 * Called by log macros after registration.
 */
void _cnanolog_log_binary(uint32_t log_id,
                          uint8_t num_args,
                          const uint8_t* arg_types,
                          ...);

/* ============================================================================
 * User-Facing Logging Macros
 * ============================================================================ */

/* Include type detection for automatic argument type inference */
#include "cnanolog_types.h"

/* Base macro for logs WITH NO arguments */
#define CNANOLOG_LOG0(level, format) \
    do { \
        static uint32_t __cnanolog_cached_id = UINT32_MAX; \
        static const uint8_t __cnanolog_empty_types[] = {0}; \
        if (__cnanolog_cached_id == UINT32_MAX) { \
            __cnanolog_cached_id = _cnanolog_register_site( \
                level, __FILE__, __LINE__, format, 0, __cnanolog_empty_types); \
        } \
        _cnanolog_log_binary(__cnanolog_cached_id, 0, __cnanolog_empty_types); \
    } while(0)

/* Base macro for logs WITH arguments */
#define CNANOLOG_LOG_ARGS(level, format, ...) \
    do { \
        static uint32_t __cnanolog_cached_id = UINT32_MAX; \
        if (__cnanolog_cached_id == UINT32_MAX) { \
            __cnanolog_cached_id = _cnanolog_register_site( \
                level, __FILE__, __LINE__, format, \
                CNANOLOG_COUNT_ARGS(__VA_ARGS__), \
                CNANOLOG_ARG_TYPES(__VA_ARGS__)); \
        } \
        _cnanolog_log_binary(__cnanolog_cached_id, \
                            CNANOLOG_COUNT_ARGS(__VA_ARGS__), \
                            CNANOLOG_ARG_TYPES(__VA_ARGS__), \
                            __VA_ARGS__); \
    } while(0)

/* ============================================================================
 * Convenient Level-Specific Macros
 *
 * Usage:
 *   log_info("message")              - No arguments
 *   log_info1("count: %d", 42)       - One argument
 *   log_info2("x=%d y=%d", x, y)     - Two arguments
 *   ... etc
 * ============================================================================ */

/* INFO - No arguments */
#define log_info(format) \
    CNANOLOG_LOG0(LOG_LEVEL_INFO, format)

/* INFO - With arguments (up to 8) */
#define log_info1(format, a1) \
    CNANOLOG_LOG_ARGS(LOG_LEVEL_INFO, format, a1)
#define log_info2(format, a1, a2) \
    CNANOLOG_LOG_ARGS(LOG_LEVEL_INFO, format, a1, a2)
#define log_info3(format, a1, a2, a3) \
    CNANOLOG_LOG_ARGS(LOG_LEVEL_INFO, format, a1, a2, a3)
#define log_info4(format, a1, a2, a3, a4) \
    CNANOLOG_LOG_ARGS(LOG_LEVEL_INFO, format, a1, a2, a3, a4)
#define log_info5(format, a1, a2, a3, a4, a5) \
    CNANOLOG_LOG_ARGS(LOG_LEVEL_INFO, format, a1, a2, a3, a4, a5)
#define log_info6(format, a1, a2, a3, a4, a5, a6) \
    CNANOLOG_LOG_ARGS(LOG_LEVEL_INFO, format, a1, a2, a3, a4, a5, a6)
#define log_info7(format, a1, a2, a3, a4, a5, a6, a7) \
    CNANOLOG_LOG_ARGS(LOG_LEVEL_INFO, format, a1, a2, a3, a4, a5, a6, a7)
#define log_info8(format, a1, a2, a3, a4, a5, a6, a7, a8) \
    CNANOLOG_LOG_ARGS(LOG_LEVEL_INFO, format, a1, a2, a3, a4, a5, a6, a7, a8)

/* WARN - No arguments */
#define log_warn(format) \
    CNANOLOG_LOG0(LOG_LEVEL_WARN, format)

/* WARN - With arguments */
#define log_warn1(format, a1) \
    CNANOLOG_LOG_ARGS(LOG_LEVEL_WARN, format, a1)
#define log_warn2(format, a1, a2) \
    CNANOLOG_LOG_ARGS(LOG_LEVEL_WARN, format, a1, a2)
#define log_warn3(format, a1, a2, a3) \
    CNANOLOG_LOG_ARGS(LOG_LEVEL_WARN, format, a1, a2, a3)
#define log_warn4(format, a1, a2, a3, a4) \
    CNANOLOG_LOG_ARGS(LOG_LEVEL_WARN, format, a1, a2, a3, a4)

/* ERROR - No arguments */
#define log_error(format) \
    CNANOLOG_LOG0(LOG_LEVEL_ERROR, format)

/* ERROR - With arguments */
#define log_error1(format, a1) \
    CNANOLOG_LOG_ARGS(LOG_LEVEL_ERROR, format, a1)
#define log_error2(format, a1, a2) \
    CNANOLOG_LOG_ARGS(LOG_LEVEL_ERROR, format, a1, a2)
#define log_error3(format, a1, a2, a3) \
    CNANOLOG_LOG_ARGS(LOG_LEVEL_ERROR, format, a1, a2, a3)
#define log_error4(format, a1, a2, a3, a4) \
    CNANOLOG_LOG_ARGS(LOG_LEVEL_ERROR, format, a1, a2, a3, a4)

/* DEBUG - No arguments */
#define log_debug(format) \
    CNANOLOG_LOG0(LOG_LEVEL_DEBUG, format)

/* DEBUG - With arguments */
#define log_debug1(format, a1) \
    CNANOLOG_LOG_ARGS(LOG_LEVEL_DEBUG, format, a1)
#define log_debug2(format, a1, a2) \
    CNANOLOG_LOG_ARGS(LOG_LEVEL_DEBUG, format, a1, a2)
#define log_debug3(format, a1, a2, a3) \
    CNANOLOG_LOG_ARGS(LOG_LEVEL_DEBUG, format, a1, a2, a3)
#define log_debug4(format, a1, a2, a3, a4) \
    CNANOLOG_LOG_ARGS(LOG_LEVEL_DEBUG, format, a1, a2, a3, a4)

#ifdef __cplusplus
}
#endif
