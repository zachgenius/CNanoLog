/*
 * High-Precision Timing Utilities
 *
 * Provides nanosecond-precision timing for benchmark measurements.
 */

#pragma once

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CPU Timestamp Counter (RDTSC)
 * ============================================================================ */

/**
 * Read CPU timestamp counter (nanosecond precision).
 * Uses RDTSC on x86/x64 for minimal overhead (~5-10ns).
 */
static inline uint64_t bench_rdtsc(void) {
#if defined(__x86_64__) || defined(__i386__)
    uint32_t lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
#elif defined(__aarch64__)
    uint64_t val;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r" (val));
    return val;
#else
    #error "Unsupported architecture for RDTSC"
#endif
}

/**
 * Read timestamp with serialization (prevents reordering).
 * Use when you need guaranteed ordering of measurements.
 */
static inline uint64_t bench_rdtsc_serialized(void) {
#if defined(__x86_64__) || defined(__i386__)
    uint32_t lo, hi;
    __asm__ __volatile__ (
        "lfence\n\t"
        "rdtsc\n\t"
        : "=a"(lo), "=d"(hi)
    );
    return ((uint64_t)hi << 32) | lo;
#else
    return bench_rdtsc();
#endif
}

/* ============================================================================
 * CPU Frequency Calibration
 * ============================================================================ */

/**
 * Calibrate CPU frequency by comparing RDTSC to wall-clock time.
 * Call once at startup.
 *
 * @return CPU frequency in Hz
 */
static inline uint64_t bench_calibrate_cpu_frequency(void) {
    struct timespec ts_start, ts_end;
    uint64_t tsc_start, tsc_end;

    /* Measure over 100ms for accuracy */
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    tsc_start = bench_rdtsc();

    struct timespec sleep_time = {0, 100000000};  /* 100ms */
    nanosleep(&sleep_time, NULL);

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    tsc_end = bench_rdtsc();

    /* Calculate frequency */
    double elapsed_sec = (ts_end.tv_sec - ts_start.tv_sec) +
                         (ts_end.tv_nsec - ts_start.tv_nsec) / 1e9;
    uint64_t elapsed_ticks = tsc_end - tsc_start;

    return (uint64_t)(elapsed_ticks / elapsed_sec);
}

/**
 * Convert TSC cycles to nanoseconds.
 *
 * @param cycles Number of CPU cycles
 * @param cpu_freq CPU frequency in Hz
 * @return Time in nanoseconds
 */
static inline double bench_cycles_to_ns(uint64_t cycles, uint64_t cpu_freq) {
    return (cycles * 1e9) / cpu_freq;
}

/* ============================================================================
 * Wall-Clock Timing (for end-to-end measurements)
 * ============================================================================ */

/**
 * Get current time in nanoseconds since epoch.
 */
static inline uint64_t bench_get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

/**
 * Get elapsed time in seconds between two timestamps.
 */
static inline double bench_elapsed_sec(uint64_t start_ns, uint64_t end_ns) {
    return (end_ns - start_ns) / 1e9;
}

/* ============================================================================
 * Latency Measurement Helpers
 * ============================================================================ */

/**
 * Measure latency of a single operation.
 * Returns latency in nanoseconds.
 */
#define BENCH_MEASURE_LATENCY(cpu_freq, operation) ({ \
    uint64_t start = bench_rdtsc(); \
    operation; \
    uint64_t end = bench_rdtsc(); \
    bench_cycles_to_ns(end - start, cpu_freq); \
})

/**
 * Measure throughput over a duration.
 * Returns operations per second.
 */
#define BENCH_MEASURE_THROUGHPUT(operations, duration_sec) \
    ((double)(operations) / (duration_sec))

#ifdef __cplusplus
}
#endif
