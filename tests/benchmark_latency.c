/*
 * CNanoLog Performance Benchmark - Phase 6.5
 *
 * Measures:
 * - Latency (cycles per log call)
 * - Throughput (logs per second)
 * - Multi-threaded scalability
 * - Preallocate API impact
 * - Compression effectiveness
 */

#include <cnanolog.h>
#include "../src/cycles.h"
#include "../src/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WARMUP_ITERATIONS 10000
#define BENCH_ITERATIONS  1000000

/* CPU frequency estimation (Hz) */
static uint64_t g_cpu_freq = 3000000000ULL;  /* 3 GHz default */

/* Calibrate CPU frequency */
void calibrate_cpu_frequency(void) {
    struct timespec start, end;
    uint64_t tsc_start, tsc_end;

    clock_gettime(CLOCK_MONOTONIC, &start);
    tsc_start = rdtsc();

    /* Sleep for 100ms */
    struct timespec sleep_time = {0, 100000000};
    nanosleep(&sleep_time, NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);
    tsc_end = rdtsc();

    uint64_t elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                          (end.tv_nsec - start.tv_nsec);
    uint64_t elapsed_cycles = tsc_end - tsc_start;

    g_cpu_freq = (elapsed_cycles * 1000000000ULL) / elapsed_ns;

    printf("CPU Frequency: %.2f GHz\n", g_cpu_freq / 1e9);
}

/* Convert cycles to nanoseconds */
double cycles_to_ns(uint64_t cycles) {
    return (cycles * 1e9) / g_cpu_freq;
}

/* ============================================================================
 * Single-Threaded Latency Benchmarks
 * ============================================================================ */

void benchmark_no_args(void) {
    uint64_t start, end;

    /* Warmup */
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        log_info("Warmup");
    }

    /* Benchmark */
    start = rdtsc();
    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        log_info("Benchmark test");
    }
    end = rdtsc();

    uint64_t total_cycles = end - start;
    uint64_t cycles_per_call = total_cycles / BENCH_ITERATIONS;

    printf("  No arguments:       %4llu cycles (%6.1f ns)\n",
           (unsigned long long)cycles_per_call,
           cycles_to_ns(cycles_per_call));
}

void benchmark_one_int(void) {
    uint64_t start, end;

    /* Warmup */
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        log_info1("Warmup %d", i);
    }

    /* Benchmark */
    start = rdtsc();
    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        log_info1("Value: %d", i);
    }
    end = rdtsc();

    uint64_t total_cycles = end - start;
    uint64_t cycles_per_call = total_cycles / BENCH_ITERATIONS;

    printf("  One integer:        %4llu cycles (%6.1f ns)\n",
           (unsigned long long)cycles_per_call,
           cycles_to_ns(cycles_per_call));
}

void benchmark_two_ints(void) {
    uint64_t start, end;

    /* Warmup */
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        log_info2("Warmup %d %d", i, i * 2);
    }

    /* Benchmark */
    start = rdtsc();
    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        log_info2("X=%d Y=%d", i, i * 2);
    }
    end = rdtsc();

    uint64_t total_cycles = end - start;
    uint64_t cycles_per_call = total_cycles / BENCH_ITERATIONS;

    printf("  Two integers:       %4llu cycles (%6.1f ns)\n",
           (unsigned long long)cycles_per_call,
           cycles_to_ns(cycles_per_call));
}

void benchmark_three_ints(void) {
    uint64_t start, end;

    /* Warmup */
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        log_info3("Warmup %d %d %d", i, i * 2, i * 3);
    }

    /* Benchmark */
    start = rdtsc();
    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        log_info3("X=%d Y=%d Z=%d", i, i * 2, i * 3);
    }
    end = rdtsc();

    uint64_t total_cycles = end - start;
    uint64_t cycles_per_call = total_cycles / BENCH_ITERATIONS;

    printf("  Three integers:     %4llu cycles (%6.1f ns)\n",
           (unsigned long long)cycles_per_call,
           cycles_to_ns(cycles_per_call));
}

void benchmark_one_string(void) {
    const char* str = "Test string";
    uint64_t start, end;

    /* Warmup */
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        log_info1("Warmup %s", str);
    }

    /* Benchmark */
    start = rdtsc();
    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        log_info1("Name: %s", str);
    }
    end = rdtsc();

    uint64_t total_cycles = end - start;
    uint64_t cycles_per_call = total_cycles / BENCH_ITERATIONS;

    printf("  One string:         %4llu cycles (%6.1f ns)\n",
           (unsigned long long)cycles_per_call,
           cycles_to_ns(cycles_per_call));
}

/* ============================================================================
 * Preallocate API Impact
 * ============================================================================ */

void benchmark_with_preallocate(void) {
    printf("\n");
    printf("Preallocate API Impact:\n");
    printf("-----------------------\n");

    /* First log WITHOUT preallocate (measures allocation overhead) */
    uint64_t start = rdtsc();
    log_info("First log");
    uint64_t end = rdtsc();
    uint64_t first_log_cycles = end - start;

    printf("  First log (no prealloc): %llu cycles (%6.1f ns)\n",
           (unsigned long long)first_log_cycles,
           cycles_to_ns(first_log_cycles));

    /* Subsequent logs are fast */
    start = rdtsc();
    log_info("Second log");
    end = rdtsc();
    uint64_t second_log_cycles = end - start;

    printf("  Second log (cached):     %llu cycles (%6.1f ns)\n",
           (unsigned long long)second_log_cycles,
           cycles_to_ns(second_log_cycles));

    printf("  Overhead avoided:        %llu cycles (%6.1f ns)\n",
           (unsigned long long)(first_log_cycles - second_log_cycles),
           cycles_to_ns(first_log_cycles - second_log_cycles));

    printf("\n  Recommendation: Call cnanolog_preallocate() at thread start\n");
}

/* ============================================================================
 * Throughput Benchmark
 * ============================================================================ */

void benchmark_throughput(void) {
    printf("\n");
    printf("Throughput (single-threaded):\n");
    printf("-----------------------------\n");

    const int iterations = 5000000;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < iterations; i++) {
        log_info1("Throughput test %d", i);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    double logs_per_sec = iterations / elapsed;

    printf("  %d logs in %.3f seconds\n", iterations, elapsed);
    printf("  Throughput: %.2f million logs/sec\n", logs_per_sec / 1e6);
}

/* ============================================================================
 * Multi-Threaded Benchmark
 * ============================================================================ */

typedef struct {
    int thread_id;
    int iterations;
    uint64_t start_time;
    uint64_t end_time;
} thread_args_t;

void* worker_thread(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;

    /* Preallocate to avoid first-log overhead */
    cnanolog_preallocate();

    /* All threads start together */
    args->start_time = rdtsc();

    for (int i = 0; i < args->iterations; i++) {
        log_info2("Thread %d: iteration %d", args->thread_id, i);
    }

    args->end_time = rdtsc();

    return NULL;
}

void benchmark_multithreaded(int num_threads) {
    printf("\n");
    printf("Multi-threaded performance (%d threads):\n", num_threads);
    printf("----------------------------------------\n");

    const int iterations_per_thread = 500000;

    thread_t* threads = malloc(num_threads * sizeof(thread_t));
    thread_args_t* args = malloc(num_threads * sizeof(thread_args_t));

    /* Create threads */
    for (int i = 0; i < num_threads; i++) {
        args[i].thread_id = i;
        args[i].iterations = iterations_per_thread;
        thread_create(&threads[i], worker_thread, &args[i]);
    }

    /* Wait for all threads */
    for (int i = 0; i < num_threads; i++) {
        thread_join(threads[i], NULL);
    }

    /* Calculate throughput */
    uint64_t min_start = args[0].start_time;
    uint64_t max_end = args[0].end_time;

    for (int i = 1; i < num_threads; i++) {
        if (args[i].start_time < min_start) min_start = args[i].start_time;
        if (args[i].end_time > max_end) max_end = args[i].end_time;
    }

    uint64_t total_cycles = max_end - min_start;
    double elapsed_sec = cycles_to_ns(total_cycles) / 1e9;
    int total_logs = num_threads * iterations_per_thread;
    double logs_per_sec = total_logs / elapsed_sec;

    printf("  Total logs: %d\n", total_logs);
    printf("  Elapsed: %.3f seconds\n", elapsed_sec);
    printf("  Throughput: %.2f million logs/sec\n", logs_per_sec / 1e6);
    printf("  Per-thread: %.2f million logs/sec\n", (logs_per_sec / num_threads) / 1e6);

    free(threads);
    free(args);
}

/* ============================================================================
 * Main Benchmark Suite
 * ============================================================================ */

int main(void) {
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║   CNanoLog Performance Benchmark - Phase 6.5         ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n\n");

    /* Initialize */
    calibrate_cpu_frequency();
    printf("\n");

    if (cnanolog_init("benchmark.clog") != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    /* Preallocate for main thread */
    cnanolog_preallocate();

    printf("Single-Threaded Latency:\n");
    printf("------------------------\n");
    benchmark_no_args();
    benchmark_one_int();
    benchmark_two_ints();
    benchmark_three_ints();
    benchmark_one_string();

    benchmark_with_preallocate();
    benchmark_throughput();

    /* Multi-threaded benchmarks */
    benchmark_multithreaded(2);
    benchmark_multithreaded(4);

    /* Wait for background thread to process */
    struct timespec sleep_time = {0, 200000000};  /* 200ms */
    nanosleep(&sleep_time, NULL);

    /* Get final statistics */
    printf("\n");
    printf("Final Statistics:\n");
    printf("-----------------\n");

    cnanolog_stats_t stats;
    cnanolog_get_stats(&stats);

    printf("  Total logs written:     %llu\n", (unsigned long long)stats.total_logs_written);
    printf("  Dropped logs:           %llu\n", (unsigned long long)stats.dropped_logs);
    printf("  Total bytes written:    %llu bytes\n", (unsigned long long)stats.total_bytes_written);
    printf("  Compression ratio:      %.2fx\n", stats.compression_ratio_x100 / 100.0);
    printf("  Staging buffers active: %llu\n", (unsigned long long)stats.staging_buffers_active);
    printf("  Background wakeups:     %llu\n", (unsigned long long)stats.background_wakeups);

    double drop_rate = (stats.dropped_logs * 100.0) / stats.total_logs_written;
    printf("  Drop rate:              %.4f%%\n", drop_rate);

    cnanolog_shutdown();

    printf("\n");
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║   Benchmark Complete!                                 ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n");

    return 0;
}
