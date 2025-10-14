/* Copyright (c) 2025
 * CNanoLog Log Site Registry Implementation
 */

#include "log_registry.h"
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 64
#define GROWTH_FACTOR 2

/* ============================================================================
 * Registry Initialization
 * ============================================================================ */

void log_registry_init(log_registry_t* registry) {
    registry->sites = (log_site_t*)calloc(INITIAL_CAPACITY, sizeof(log_site_t));
    registry->count = 0;
    registry->capacity = INITIAL_CAPACITY;
    cnanolog_mutex_init(&registry->lock);
}

/* ============================================================================
 * Registry Registration
 * ============================================================================ */

/**
 * Check if a site with matching file:line:format already exists.
 * Returns log_id if found, UINT32_MAX if not found.
 */
static uint32_t find_existing_site(const log_registry_t* registry,
                                    const char* filename,
                                    uint32_t line_number,
                                    const char* format) {
    for (uint32_t i = 0; i < registry->count; i++) {
        const log_site_t* site = &registry->sites[i];
        if (site->line_number == line_number &&
            strcmp(site->filename, filename) == 0 &&
            strcmp(site->format, format) == 0) {
            return site->log_id;
        }
    }
    return UINT32_MAX;
}

/**
 * Grow the registry capacity if needed.
 */
static int grow_if_needed(log_registry_t* registry) {
    if (registry->count >= registry->capacity) {
        uint32_t new_capacity = registry->capacity * GROWTH_FACTOR;
        log_site_t* new_sites = (log_site_t*)realloc(registry->sites,
                                                      new_capacity * sizeof(log_site_t));
        if (new_sites == NULL) {
            return -1;  /* Out of memory */
        }

        /* Zero out new space */
        memset(new_sites + registry->capacity, 0,
               (new_capacity - registry->capacity) * sizeof(log_site_t));

        registry->sites = new_sites;
        registry->capacity = new_capacity;
    }
    return 0;
}

uint32_t log_registry_register(log_registry_t* registry,
                                cnanolog_level_t level,
                                const char* filename,
                                uint32_t line_number,
                                const char* format,
                                uint8_t num_args,
                                const uint8_t* arg_types) {
    cnanolog_mutex_lock(&registry->lock);

    /* Check if this site already exists */
    uint32_t existing_id = find_existing_site(registry, filename, line_number, format);
    if (existing_id != UINT32_MAX) {
        cnanolog_mutex_unlock(&registry->lock);
        return existing_id;
    }

    /* Grow if needed */
    if (grow_if_needed(registry) != 0) {
        cnanolog_mutex_unlock(&registry->lock);
        return UINT32_MAX;  /* Failed to allocate */
    }

    /* Add new site */
    uint32_t new_id = registry->count;
    log_site_t* site = &registry->sites[new_id];

    site->log_id = new_id;
    site->log_level = level;
    site->filename = filename;  /* Note: assumes string literals (static storage) */
    site->format = format;      /* Note: assumes string literals (static storage) */
    site->line_number = line_number;
    site->num_args = num_args;

    /* Copy argument types (convert uint8_t -> cnanolog_arg_type_t) */
    for (uint8_t i = 0; i < num_args; i++) {
        site->arg_types[i] = (cnanolog_arg_type_t)arg_types[i];
    }
    /* Zero remaining elements */
    for (uint8_t i = num_args; i < CNANOLOG_MAX_ARGS; i++) {
        site->arg_types[i] = ARG_TYPE_NONE;
    }

    registry->count++;

    cnanolog_mutex_unlock(&registry->lock);
    return new_id;
}

/* ============================================================================
 * Registry Query
 * ============================================================================ */

const log_site_t* log_registry_get(const log_registry_t* registry, uint32_t log_id) {
    if (log_id >= registry->count) {
        return NULL;
    }
    return &registry->sites[log_id];
}

uint32_t log_registry_count(const log_registry_t* registry) {
    return registry->count;
}

const log_site_t* log_registry_get_all(const log_registry_t* registry, uint32_t* out_count) {
    if (out_count != NULL) {
        *out_count = registry->count;
    }
    return registry->sites;
}

/* ============================================================================
 * Registry Cleanup
 * ============================================================================ */

void log_registry_destroy(log_registry_t* registry) {
    if (registry->sites != NULL) {
        free(registry->sites);
        registry->sites = NULL;
    }
    registry->count = 0;
    registry->capacity = 0;
    cnanolog_mutex_destroy(&registry->lock);
}
