/* Copyright (c) 2025
 * CNanoLog CPU Cycle Counter
 *
 * High-performance timestamp source using CPU cycle counters (RDTSC).
 * Provides nanosecond precision with ~5-10ns overhead (vs ~100-200ns for clock_gettime).
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CPU Cycle Counter API
 * ============================================================================ */

/**
 * Read CPU cycle counter (RDTSC on x86_64, CNTVCT on ARM64).
 *
 * This function has extremely low overhead (~20 cycles) compared to
 * system calls like clock_gettime() (~1000+ cycles).
 *
 * Requirements:
 * - Modern CPUs (2008+) with constant/invariant TSC
 * - Synchronized TSC across all cores (true on all modern systems)
 *
 * Returns:
 *   uint64_t - Current CPU cycle count
 *
 * Note: The returned value is in CPU cycles, not nanoseconds.
 *       Use calibration data to convert to wall-clock time.
 */
static inline uint64_t rdtsc(void) {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    /* x86/x86_64: Read Time-Stamp Counter */
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;

#elif defined(__aarch64__) || defined(_M_ARM64)
    /* ARM64: Read Virtual Count Register */
    uint64_t val;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(val));
    return val;

#elif defined(__arm__)
    /* ARM32: Read Cycle Counter (requires kernel config) */
    uint32_t val;
    __asm__ __volatile__("mrc p15, 0, %0, c9, c13, 0" : "=r"(val));
    return (uint64_t)val;

#else
    /* Unsupported architecture: Fall back to portable method */
    #warning "rdtsc() not supported on this architecture, using fallback"
    #include <time.h>
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

/**
 * Serializing RDTSC: Forces completion of all prior instructions before reading TSC.
 *
 * Use this when you need a precise measurement barrier (e.g., benchmarking).
 * For logging, the non-serializing rdtsc() is sufficient and faster.
 */
static inline uint64_t rdtscp(void) {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    /* RDTSCP: Serialized version of RDTSC */
    uint32_t lo, hi;
    uint32_t aux;  /* Unused but required by instruction */
    __asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux));
    return ((uint64_t)hi << 32) | lo;
#else
    /* Fall back to regular rdtsc on architectures without serializing variant */
    return rdtsc();
#endif
}

/**
 * CPU frequency fence: Prevent reordering of instructions around timestamp reads.
 *
 * Use before/after rdtsc() when doing precise measurements.
 */
static inline void cpu_fence(void) {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    /* CPUID acts as a serializing instruction */
    uint32_t eax = 0, ebx, ecx, edx;
    __asm__ __volatile__("cpuid"
                        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                        : "a"(eax));
#elif defined(__aarch64__) || defined(_M_ARM64)
    /* ISB: Instruction Synchronization Barrier */
    __asm__ __volatile__("isb" ::: "memory");
#else
    /* Compiler barrier */
    __asm__ __volatile__("" ::: "memory");
#endif
}

#ifdef __cplusplus
}
#endif
