/* Copyright (c) 2025
 * CNanoLog Log Site Registry
 *
 * Manages registration and metadata for log call sites.
 * Each unique log site (file:line:format) gets a unique log_id.
 */

#pragma once

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
 * Used both for runtime registry and for writing to dictionary.
 */
typedef struct {
    uint32_t log_id;
    cnanolog_level_t log_level;
    const char* filename;
    const char* format;
    uint32_t line_number;
    uint8_t num_args;
    cnanolog_arg_type_t arg_types[CNANOLOG_MAX_ARGS];
    const char* text_pattern;  /* Custom text pattern (NULL = use global pattern) */
} log_site_t;

/* ============================================================================
 * Log Registry
 * ============================================================================ */

/**
 * Registry that stores all log sites.
 * Thread-safe for concurrent registration and lookup.
 */
typedef struct {
    log_site_t* sites;       /* Array of registered sites */
    uint32_t count;          /* Number of registered sites */
    uint32_t capacity;       /* Allocated capacity */
    cnanolog_mutex_t lock;   /* Protects concurrent registration */
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
 * @param arg_types Array of uint8_t enum values (space-efficient from macros)
 * @param text_pattern Custom text pattern (NULL = use global pattern)
 *
 * Returns: Unique log_id for this site
 */
uint32_t log_registry_register(log_registry_t* registry,
                                cnanolog_level_t level,
                                const char* filename,
                                uint32_t line_number,
                                const char* format,
                                uint8_t num_args,
                                const uint8_t* arg_types,
                                const char* text_pattern);

/**
 * Copy log site information for a log_id into caller-owned storage.
 * Returns 0 on success, -1 if log_id is invalid.
 *
 * The copied string pointers remain valid as long as the registered string
 * literals remain valid.
 */
int log_registry_get(log_registry_t* registry, uint32_t log_id, log_site_t* out_site);

/**
 * Get the total number of registered sites.
 */
uint32_t log_registry_count(log_registry_t* registry);

/**
 * Lock the registry and get all registered sites for dictionary writing.
 * The returned pointer remains valid until log_registry_unlock() is called.
 */
const log_site_t* log_registry_lock_all(log_registry_t* registry, uint32_t* out_count);

/**
 * Unlock a registry previously locked by log_registry_lock_all().
 */
void log_registry_unlock(log_registry_t* registry);

/**
 * Clean up the registry.
 */
void log_registry_destroy(log_registry_t* registry);

#ifdef __cplusplus
}
#endif
