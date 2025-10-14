/*
 * Statistical Analysis Utilities
 *
 * Provides functions for calculating percentiles, mean, stddev, etc.
 */

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Latency Histogram
 * ============================================================================ */

#define MAX_SAMPLES 10000000  /* 10M samples max */

typedef struct {
    uint64_t* samples;
    size_t count;
    size_t capacity;
    int sorted;
} latency_histogram_t;

/**
 * Initialize latency histogram.
 */
static inline latency_histogram_t* latency_histogram_create(size_t capacity) {
    latency_histogram_t* hist = (latency_histogram_t*)malloc(sizeof(latency_histogram_t));
    if (!hist) return NULL;

    hist->samples = (uint64_t*)malloc(capacity * sizeof(uint64_t));
    if (!hist->samples) {
        free(hist);
        return NULL;
    }

    hist->count = 0;
    hist->capacity = capacity;
    hist->sorted = 0;

    return hist;
}

/**
 * Free histogram.
 */
static inline void latency_histogram_destroy(latency_histogram_t* hist) {
    if (hist) {
        free(hist->samples);
        free(hist);
    }
}

/**
 * Add sample to histogram.
 */
static inline void latency_histogram_add(latency_histogram_t* hist, uint64_t value) {
    if (hist->count < hist->capacity) {
        hist->samples[hist->count++] = value;
        hist->sorted = 0;
    }
}

/**
 * Comparison function for qsort.
 */
static int compare_uint64(const void* a, const void* b) {
    uint64_t ua = *(const uint64_t*)a;
    uint64_t ub = *(const uint64_t*)b;
    if (ua < ub) return -1;
    if (ua > ub) return 1;
    return 0;
}

/**
 * Sort samples (required before calculating percentiles).
 */
static inline void latency_histogram_sort(latency_histogram_t* hist) {
    if (!hist->sorted && hist->count > 0) {
        qsort(hist->samples, hist->count, sizeof(uint64_t), compare_uint64);
        hist->sorted = 1;
    }
}

/**
 * Get percentile value.
 *
 * @param hist Histogram (will be sorted if not already)
 * @param percentile 0.0 to 100.0 (e.g., 99.9 for p99.9)
 * @return Value at that percentile
 */
static inline uint64_t latency_histogram_percentile(latency_histogram_t* hist,
                                                     double percentile) {
    if (hist->count == 0) return 0;

    latency_histogram_sort(hist);

    size_t index = (size_t)((percentile / 100.0) * hist->count);
    if (index >= hist->count) index = hist->count - 1;

    return hist->samples[index];
}

/**
 * Calculate mean (average).
 */
static inline double latency_histogram_mean(latency_histogram_t* hist) {
    if (hist->count == 0) return 0.0;

    uint64_t sum = 0;
    for (size_t i = 0; i < hist->count; i++) {
        sum += hist->samples[i];
    }

    return (double)sum / hist->count;
}

/**
 * Calculate standard deviation.
 */
static inline double latency_histogram_stddev(latency_histogram_t* hist) {
    if (hist->count < 2) return 0.0;

    double mean = latency_histogram_mean(hist);
    double sum_sq_diff = 0.0;

    for (size_t i = 0; i < hist->count; i++) {
        double diff = hist->samples[i] - mean;
        sum_sq_diff += diff * diff;
    }

    return sqrt(sum_sq_diff / (hist->count - 1));
}

/* ============================================================================
 * Summary Statistics
 * ============================================================================ */

typedef struct {
    uint64_t min;
    uint64_t max;
    double mean;
    double stddev;
    uint64_t p50;
    uint64_t p95;
    uint64_t p99;
    uint64_t p999;
} summary_stats_t;

/**
 * Calculate all summary statistics at once.
 */
static inline void latency_histogram_summary(latency_histogram_t* hist,
                                              summary_stats_t* stats) {
    if (hist->count == 0) {
        memset(stats, 0, sizeof(summary_stats_t));
        return;
    }

    latency_histogram_sort(hist);

    stats->min = hist->samples[0];
    stats->max = hist->samples[hist->count - 1];
    stats->mean = latency_histogram_mean(hist);
    stats->stddev = latency_histogram_stddev(hist);
    stats->p50 = latency_histogram_percentile(hist, 50.0);
    stats->p95 = latency_histogram_percentile(hist, 95.0);
    stats->p99 = latency_histogram_percentile(hist, 99.0);
    stats->p999 = latency_histogram_percentile(hist, 99.9);
}

/* ============================================================================
 * Running Statistics (for online calculation)
 * ============================================================================ */

typedef struct {
    uint64_t count;
    double mean;
    double m2;     /* For Welford's algorithm */
    uint64_t min;
    uint64_t max;
} running_stats_t;

/**
 * Initialize running statistics.
 */
static inline void running_stats_init(running_stats_t* stats) {
    stats->count = 0;
    stats->mean = 0.0;
    stats->m2 = 0.0;
    stats->min = UINT64_MAX;
    stats->max = 0;
}

/**
 * Update running statistics with new value.
 * Uses Welford's online algorithm for numerical stability.
 */
static inline void running_stats_update(running_stats_t* stats, uint64_t value) {
    stats->count++;

    /* Update min/max */
    if (value < stats->min) stats->min = value;
    if (value > stats->max) stats->max = value;

    /* Welford's algorithm for mean and variance */
    double delta = value - stats->mean;
    stats->mean += delta / stats->count;
    double delta2 = value - stats->mean;
    stats->m2 += delta * delta2;
}

/**
 * Get mean from running statistics.
 */
static inline double running_stats_mean(const running_stats_t* stats) {
    return stats->mean;
}

/**
 * Get standard deviation from running statistics.
 */
static inline double running_stats_stddev(const running_stats_t* stats) {
    if (stats->count < 2) return 0.0;
    return sqrt(stats->m2 / (stats->count - 1));
}

#ifdef __cplusplus
}
#endif
