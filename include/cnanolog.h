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
        const uint8_t* __cnanolog_arg_types = CNANOLOG_ARG_TYPES(__VA_ARGS__); \
        uint8_t __cnanolog_num_args = CNANOLOG_COUNT_ARGS(__VA_ARGS__); \
        if (__cnanolog_cached_id == UINT32_MAX) { \
            __cnanolog_cached_id = _cnanolog_register_site( \
                level, __FILE__, __LINE__, format, \
                __cnanolog_num_args, \
                __cnanolog_arg_types); \
        } \
        _cnanolog_log_binary(__cnanolog_cached_id, \
                            __cnanolog_num_args, \
                            __cnanolog_arg_types, \
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

/* ============================================================================
 * Statistics and Monitoring
 * ============================================================================ */

#ifdef __cplusplus
}
#endif
