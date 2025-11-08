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
 * Output format for log files.
 */
typedef enum {
    CNANOLOG_OUTPUT_BINARY = 0,  /* Binary format (default) - requires decompressor */
    CNANOLOG_OUTPUT_TEXT = 1,    /* Human-readable text format - no decompressor needed */
} cnanolog_output_format_t;

/**
 * Log rotation policy for date-based rotation.
 */
typedef enum {
    CNANOLOG_ROTATE_NONE = 0,    /* No rotation (default) */
    CNANOLOG_ROTATE_DAILY,        /* Rotate when date changes */
} cnanolog_rotation_policy_t;

/**
 * Configuration for log rotation and output format.
 */
typedef struct {
    cnanolog_rotation_policy_t policy;  /* Rotation policy */
    const char* base_path;               /* Base path for log files (e.g., "app.clog") */
                                         /* Dated files: "app-2025-11-02.clog" */
    cnanolog_output_format_t format;     /* Output format (binary or text) */
                                         /* Default: CNANOLOG_OUTPUT_BINARY */
    const char* text_pattern;            /* Text format pattern (NULL = use default) */
                                         /* Only applies when format == CNANOLOG_OUTPUT_TEXT */
                                         /* Default: "[%t] [%l] [%f:%n] %m" */
} cnanolog_rotation_config_t;

/**
 * Text format pattern tokens (used when format == CNANOLOG_OUTPUT_TEXT):
 *
 *   %t - Full timestamp (YYYY-MM-DD HH:MM:SS.nnnnnnnnn)
 *   %T - Short timestamp (HH:MM:SS.nnn) - microsecond precision
 *   %d - Date only (YYYY-MM-DD)
 *   %D - Time only (HH:MM:SS)
 *   %l - Log level name (INFO, WARN, ERROR, DEBUG)
 *   %L - Log level letter (I, W, E, D)
 *   %f - Filename (basename)
 *   %F - Full file path
 *   %n - Line number
 *   %m - Formatted message
 *   %% - Literal %
 *
 * Examples:
 *   "[%t] [%l] [%f:%n] %m"                      - Default (full info)
 *   "%t [%l] %m"                                - Simple (no file info)
 *   "%T %L %m"                                  - Compact
 *   "{\"time\":\"%t\",\"level\":\"%l\",\"msg\":\"%m\"}" - JSON
 *   "time=%t level=%l file=%f:%n msg=\"%m\""    - Logfmt
 */
#define CNANOLOG_DEFAULT_PATTERN "[%t] [%l] [%f:%n] %m"

/**
 * Initialize the logging system with binary format.
 * Must be called once at application startup.
 *
 * @param log_file_path Path to binary log file (*.clog)
 * @return 0 on success, -1 on failure
 */
int cnanolog_init(const char* log_file_path);

/**
 * Initialize the logging system with rotation and format options.
 * When rotation is enabled, log files are named with dates:
 * - base_path "logs/app.clog" becomes "logs/app-2025-11-02.clog"
 * - Files rotate automatically at midnight (local time)
 *
 * @param config Configuration (rotation policy and output format)
 * @return 0 on success, -1 on failure
 *
 * Example (binary format with daily rotation):
 *   cnanolog_rotation_config_t config = {
 *       .policy = CNANOLOG_ROTATE_DAILY,
 *       .base_path = "logs/app.clog",
 *       .format = CNANOLOG_OUTPUT_BINARY  // Default
 *   };
 *   cnanolog_init_ex(&config);
 *
 * Example (human-readable text format):
 *   cnanolog_rotation_config_t config = {
 *       .policy = CNANOLOG_ROTATE_NONE,
 *       .base_path = "logs/app.log",
 *       .format = CNANOLOG_OUTPUT_TEXT,  // No decompressor needed
 *       .text_pattern = NULL             // NULL = use default pattern
 *   };
 *   cnanolog_init_ex(&config);
 *
 * Example (custom text format - compact):
 *   cnanolog_rotation_config_t config = {
 *       .policy = CNANOLOG_ROTATE_NONE,
 *       .base_path = "logs/app.log",
 *       .format = CNANOLOG_OUTPUT_TEXT,
 *       .text_pattern = "%T [%l] %m"     // HH:MM:SS [INFO] message
 *   };
 *   cnanolog_init_ex(&config);
 *
 * Example (JSON format):
 *   cnanolog_rotation_config_t config = {
 *       .policy = CNANOLOG_ROTATE_NONE,
 *       .base_path = "logs/app.log",
 *       .format = CNANOLOG_OUTPUT_TEXT,
 *       .text_pattern = "{\"time\":\"%t\",\"level\":\"%l\",\"msg\":\"%m\"}"
 *   };
 *   cnanolog_init_ex(&config);
 *
 */
int cnanolog_init_ex(const cnanolog_rotation_config_t* config);

/**
 * Shut down the logger, flush all messages, and write dictionary.
 */
void cnanolog_shutdown(void);

/* ============================================================================
 * Statistics & Monitoring
 * ============================================================================ */

/**
 * Runtime statistics for the logging system.
 */
typedef struct {
    uint64_t total_logs_written;     /* Total log entries written */
    uint64_t total_bytes_written;    /* Total bytes written to file */
    uint64_t dropped_logs;           /* Logs dropped due to full buffers */
    uint64_t compression_ratio_x100; /* e.g., 350 = 3.50x compression */
    uint64_t staging_buffers_active; /* Number of thread-local buffers */
    uint64_t background_wakeups;     /* Background thread wake count */
} cnanolog_stats_t;

/**
 * Get current logging statistics.
 * Thread-safe, can be called at any time.
 *
 * @param stats Pointer to structure to fill with statistics
 */
void cnanolog_get_stats(cnanolog_stats_t* stats);

/**
 * Reset statistics counters to zero.
 * Does not affect operational state, only counters.
 */
void cnanolog_reset_stats(void);

/**
 * Preallocate thread-local buffer for the calling thread.
 * Call this before any logging to avoid first-log allocation overhead.
 *
 * This is optional but recommended for performance-critical threads.
 */
void cnanolog_preallocate(void);

/**
 * Set CPU affinity for the background writer thread.
 * Binds the writer thread to a specific CPU core for better cache locality
 * and reduced thread migration overhead.
 *
 * IMPORTANT: Call this AFTER cnanolog_init() to bind the background thread.
 *
 * Benefits:
 * - Improved cache locality (L1/L2/L3 caches stay warm)
 * - Eliminates thread migration overhead (~1000-5000 cycles)
 * - More predictable latency
 * - Better for NUMA systems
 *
 * Platform notes:
 * - Linux: Uses pthread_setaffinity_np() - direct core binding
 * - macOS: Uses thread_policy_set() - affinity tags (best-effort)
 * - Windows: Uses SetThreadAffinityMask() - direct core binding
 *
 * @param core_id CPU core ID (0-based, typically 0 to num_cores-1)
 * @return 0 on success, -1 on failure
 *
 * Example:
 *   cnanolog_init("app.clog");
 *   cnanolog_set_writer_affinity(7);  // Pin to core 7 (isolated core)
 */
int cnanolog_set_writer_affinity(int core_id);

/* ============================================================================
 * Log Levels
 * ============================================================================ */

typedef enum {
    LOG_LEVEL_INFO  = 0,
    LOG_LEVEL_WARN  = 1,
    LOG_LEVEL_ERROR = 2,
    LOG_LEVEL_DEBUG = 3
} cnanolog_level_t;

/* Custom log levels can use any uint8_t value (4-255) */
#define CNANOLOG_MAX_CUSTOM_LEVELS 64

/**
 * Register a custom log level.
 * Must be called before cnanolog_init() or cnanolog_init_ex().
 *
 * @param name Level name (e.g., "METRIC", "AUDIT", "TRACE")
 * @param level Level value (4-255, 0-3 are reserved for INFO/WARN/ERROR/DEBUG)
 * @return 0 on success, -1 on failure
 *
 * Example:
 *   cnanolog_register_level("METRIC", 10);
 *   cnanolog_register_level("AUDIT", 20);
 *   cnanolog_register_level("TRACE", 5);
 *
 * Then use with cnanolog_log() or define convenience macros:
 *   #define log_metric(fmt, ...) cnanolog_log(10, fmt, ##__VA_ARGS__)
 */
int cnanolog_register_level(const char* name, uint8_t level);

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
        static uint8_t __cnanolog_arg_types[] = CNANOLOG_ARG_TYPES(__VA_ARGS__); \
        static const uint8_t __cnanolog_num_args = CNANOLOG_COUNT_ARGS(__VA_ARGS__); \
        if (__cnanolog_cached_id == UINT32_MAX) { \
            __cnanolog_cached_id = _cnanolog_register_site( \
                level, __FILE__, __LINE__, format, \
                __cnanolog_num_args, \
                __cnanolog_arg_types); \
        } \
        _cnanolog_log_binary(__cnanolog_cached_id, \
                            __cnanolog_num_args, \
                            __cnanolog_arg_types, \
                            ##__VA_ARGS__); \
    } while(0)

/* ============================================================================
 * Logging Macros
 *
 * These variadic macros automatically handle any number of arguments (0-50).
 * No need to count arguments or use numbered versions!
 *
 * Usage:
 *   LOG_INFO("message")                    // 0 args
 *   LOG_INFO("count: %d", 42)              // 1 arg
 *   LOG_INFO("x=%d y=%d z=%d", x, y, z)    // 3 args
 *   LOG_INFO("50 args: %d %d...", 1, ...)  // up to 50 args
 * ============================================================================ */
#define LOG_INFO(format, ...) \
    CNANOLOG_LOG_ARGS(LOG_LEVEL_INFO, format, ##__VA_ARGS__)

#define LOG_WARN(format, ...) \
    CNANOLOG_LOG_ARGS(LOG_LEVEL_WARN, format, ##__VA_ARGS__)

#define LOG_ERROR(format, ...) \
    CNANOLOG_LOG_ARGS(LOG_LEVEL_ERROR, format, ##__VA_ARGS__)

#define LOG_DEBUG(format, ...) \
    CNANOLOG_LOG_ARGS(LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)

/* For custom log levels, use CNANOLOG_LOG(level, format, ...)
 * Example:
 *   cnanolog_register_level("METRIC", 10);
 *   CNANOLOG_LOG(10, "CPU: %d%%", usage);
 */
#define CNANOLOG_LOG(level, format, ...) \
    CNANOLOG_LOG_ARGS(level, format, ##__VA_ARGS__)

/* ============================================================================
 * Statistics and Monitoring
 * ============================================================================ */

#ifdef __cplusplus
}
#endif
