/* Copyright (c) 2025
 * CNanoLog Log Site Registry
 *
 * Manages registration and metadata for log call sites.
 * Each unique log site (file:line:format) gets a unique log_id.
 */

#ifndef CNANOLOG_LOG_REGISTRY_H
#define CNANOLOG_LOG_REGISTRY_H

#include "../include/cnanolog.h"
#include "../include/cnanolog_format.h"
#include "platform.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Log Site Information
 * ============================================================================ */

/**
 * Information about a single log call site.
 * Matches log_site_info_t from binary_writer.h
 */
typedef struct {
    uint32_t log_id;
    cnanolog_level_t log_level;
    const char* filename;
    const char* format;
    uint32_t line_number;
    uint8_t num_args;
    uint8_t arg_types[CNANOLOG_MAX_ARGS];
} log_site_t;

/* ============================================================================
 * Log Registry
 * ============================================================================ */

/**
 * Registry that stores all log sites.
 * Thread-safe for concurrent registration.
 */
typedef struct {
    log_site_t* sites;       /* Array of registered sites */
    uint32_t count;          /* Number of registered sites */
    uint32_t capacity;       /* Allocated capacity */
    mutex_t lock;            /* Protects concurrent registration */
} log_registry_t;

/* ============================================================================
 * Registry API
 * ============================================================================ */

/**
 * Initialize the log registry.
 * Must be called before any registration.
 */
void log_registry_init(log_registry_t* registry);

/**
 * Register a new log site and return its unique log_id.
 * Thread-safe. If the exact same site (file:line:format) is registered
 * multiple times, returns the same log_id.
 *
 * Returns: Unique log_id for this site
 */
uint32_t log_registry_register(log_registry_t* registry,
                                cnanolog_level_t level,
                                const char* filename,
                                uint32_t line_number,
                                const char* format,
                                uint8_t num_args,
                                const uint8_t* arg_types);

/**
 * Get log site information by log_id.
 * Returns NULL if log_id is invalid.
 *
 * Note: Returned pointer is valid as long as registry exists.
 */
const log_site_t* log_registry_get(const log_registry_t* registry, uint32_t log_id);

/**
 * Get the total number of registered sites.
 */
uint32_t log_registry_count(const log_registry_t* registry);

/**
 * Get all registered sites (for dictionary writing).
 * Returns pointer to internal array. Valid as long as registry exists.
 */
const log_site_t* log_registry_get_all(const log_registry_t* registry, uint32_t* out_count);

/**
 * Clean up the registry.
 */
void log_registry_destroy(log_registry_t* registry);

#ifdef __cplusplus
}
#endif

#endif /* CNANOLOG_LOG_REGISTRY_H */
