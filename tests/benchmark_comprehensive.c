/*
 * CNanoLog Comprehensive Performance Benchmark
 *
 * Tests performance from small to extreme scales (up to 10GB+ of log data)
 *
 * Measures:
 * - Latency (min/avg/max/p50/p95/p99)
 * - Throughput (logs/sec, MB/sec)
 * - Drop rates at different scales
 * - Memory usage and file sizes
 * - Compression ratios
 * - Single-threaded vs multi-threaded
 * - With/without CPU affinity
 * - Sustained performance under load
 */

#include <cnanolog.h>
#include "../src/cycles.h"
#include "../src/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/resource.h>

/* ============================================================================
 * Configuration
 * ============================================================================ */

#define MAX_SAMPLES 1000000  /* For latency histogram */

/* Test scales - can be configured via command line */
typedef struct {
    const char* name;
    uint64_t num_logs;
    int warmup_logs;
    int enabled;
} scale_config_t;

static scale_config_t g_scales[] = {
    {"Tiny",      1000,       100,  1},  /* ~10KB */
    {"Small",     10000,      1000, 1},  /* ~100KB */
    {"Medium",    100000,     5000, 1},  /* ~1MB */
    {"Large",     1000000,    10000,1},  /* ~10MB */
    {"XLarge",    10000000,   10000,1},  /* ~100MB */
    {"Huge",      100000000,  10000,1},  /* ~1GB */
    {"Extreme",   1000000000, 10000,0},  /* ~10GB - disabled by default */
};

static int g_num_scales = sizeof(g_scales) / sizeof(g_scales[0]);

/* CPU frequency */
static uint64_t g_cpu_freq = 3000000000ULL;

/* Latency samples for histogram */
static uint64_t* g_latency_samples = NULL;
static size_t g_num_samples = 0;

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

void calibrate_cpu_frequency(void) {
    struct timespec start, end;
    uint64_t tsc_start, tsc_end;

    clock_gettime(CLOCK_MONOTONIC, &start);
    tsc_start = rdtsc();

    struct timespec sleep_time = {0, 100000000};  /* 100ms */
    nanosleep(&sleep_time, NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);
    tsc_end = rdtsc();

    uint64_t elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                          (end.tv_nsec - start.tv_nsec);
    uint64_t elapsed_cycles = tsc_end - tsc_start;

    g_cpu_freq = (elapsed_cycles * 1000000000ULL) / elapsed_ns;
}

double cycles_to_ns(uint64_t cycles) {
    return (cycles * 1e9) / g_cpu_freq;
}

double cycles_to_us(uint64_t cycles) {
    return (cycles * 1e6) / g_cpu_freq;
}

/* Get file size in bytes */
uint64_t get_file_size(const char* filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_size;
    }
    return 0;
}

/* Get memory usage in KB */
uint64_t get_memory_usage(void) {
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return usage.ru_maxrss;  /* KB on Linux, bytes on macOS */
    }
    return 0;
}

/* Format bytes */
const char* format_bytes(uint64_t bytes) {
    static char buf[64];
    if (bytes < 1024) {
        snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buf, sizeof(buf), "%.2f KB", bytes / 1024.0);
    } else if (bytes < 1024 * 1024 * 1024) {
        snprintf(buf, sizeof(buf), "%.2f MB", bytes / (1024.0 * 1024.0));
    } else {
        snprintf(buf, sizeof(buf), "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    }
    return buf;
}

/* Format number with commas */
const char* format_number(uint64_t num) {
    static char buf[64];
    char temp[64];
    snprintf(temp, sizeof(temp), "%llu", (unsigned long long)num);

    int len = strlen(temp);
    int comma_count = (len - 1) / 3;
    int new_len = len + comma_count;

    buf[new_len] = '\0';

    int src = len - 1;
    int dst = new_len - 1;
    int digit_count = 0;

    while (src >= 0) {
        buf[dst--] = temp[src--];
        digit_count++;
        if (digit_count == 3 && src >= 0) {
            buf[dst--] = ',';
            digit_count = 0;
        }
    }

    return buf;
}

/* ============================================================================
 * Latency Histogram
 * ============================================================================ */

void reset_latency_samples(void) {
    g_num_samples = 0;
}

void record_latency_sample(uint64_t cycles) {
    if (g_num_samples < MAX_SAMPLES) {
        g_latency_samples[g_num_samples++] = cycles;
    }
}

/* Comparison function for qsort */
int compare_uint64(const void* a, const void* b) {
    uint64_t ua = *(const uint64_t*)a;
    uint64_t ub = *(const uint64_t*)b;
    if (ua < ub) return -1;
    if (ua > ub) return 1;
    return 0;
}

/* Calculate percentiles */
void calculate_percentiles(uint64_t* min, uint64_t* p50, uint64_t* p95,
                          uint64_t* p99, uint64_t* p999, uint64_t* max) {
    if (g_num_samples == 0) {
        *min = *p50 = *p95 = *p99 = *p999 = *max = 0;
        return;
    }

    /* Sort samples */
    qsort(g_latency_samples, g_num_samples, sizeof(uint64_t), compare_uint64);

    *min = g_latency_samples[0];
    *max = g_latency_samples[g_num_samples - 1];
    *p50 = g_latency_samples[g_num_samples * 50 / 100];
    *p95 = g_latency_samples[g_num_samples * 95 / 100];
    *p99 = g_latency_samples[g_num_samples * 99 / 100];
    *p999 = g_latency_samples[g_num_samples * 999 / 1000];
}

/* ============================================================================
 * Single-Threaded Benchmark
 * ============================================================================ */

typedef struct {
    uint64_t num_logs;
    double elapsed_sec;
    uint64_t file_size_bytes;
    uint64_t memory_kb;
    double logs_per_sec;
    double mb_per_sec;
    double compression_ratio;
    uint64_t dropped_logs;
    double drop_rate_percent;

    /* Latency stats in nanoseconds */
    double latency_min_ns;
    double latency_p50_ns;
    double latency_p95_ns;
    double latency_p99_ns;
    double latency_p999_ns;
    double latency_max_ns;
    double latency_avg_ns;
} benchmark_result_t;

void run_single_threaded_benchmark(scale_config_t* scale, benchmark_result_t* result, int cpu_affinity_core) {
    const char* log_file = "benchmark_temp.clog";

    printf("  Testing %s scale: %s logs...\n",
           scale->name, format_number(scale->num_logs));

    /* Initialize fresh logger */
    if (cnanolog_init(log_file) != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return;
    }

    /* Set CPU affinity if requested */
    if (cpu_affinity_core >= 0) {
        if (cnanolog_set_writer_affinity(cpu_affinity_core) != 0) {
            fprintf(stderr, "Warning: Failed to set CPU affinity to core %d\n", cpu_affinity_core);
        }
    }

    cnanolog_preallocate();

    /* Warmup */
    for (int i = 0; i < scale->warmup_logs; i++) {
        LOG_INFO("Warmup %d", i);
    }

    reset_latency_samples();

    /* Benchmark */
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    uint64_t sample_interval = scale->num_logs / MAX_SAMPLES;
    if (sample_interval == 0) sample_interval = 1;

    for (uint64_t i = 0; i < scale->num_logs; i++) {
        uint64_t start_cycles = rdtsc();
        LOG_INFO("Benchmark iteration %d with values %d and %d", (int)i, (int)(i * 2), (int)(i * 3));
        uint64_t end_cycles = rdtsc();

        if (i % sample_interval == 0) {
            record_latency_sample(end_cycles - start_cycles);
        }

        /* Progress indicator for very long tests */
        if (scale->num_logs > 10000000 && i % 10000000 == 0 && i > 0) {
            printf("    Progress: %s / %s (%.1f%%)\n",
                   format_number(i), format_number(scale->num_logs),
                   (i * 100.0) / scale->num_logs);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end_time);

    /* Wait for background thread to process logs */
    /* Scale wait time based on log volume */
    int wait_ms = 500;  /* Base wait */
    if (scale->num_logs > 1000000) {
        wait_ms = 2000;  /* 2 seconds for large scales */
    }
    if (scale->num_logs > 10000000) {
        wait_ms = 5000;  /* 5 seconds for very large scales */
    }

    struct timespec sleep_time = {wait_ms / 1000, (wait_ms % 1000) * 1000000};
    nanosleep(&sleep_time, NULL);

    /* Force a final flush by getting stats multiple times */
    cnanolog_stats_t stats;
    for (int i = 0; i < 3; i++) {
        cnanolog_get_stats(&stats);
        struct timespec short_wait = {0, 50000000};  /* 50ms */
        nanosleep(&short_wait, NULL);
    }

    /* Calculate results */
    result->num_logs = scale->num_logs;
    result->elapsed_sec = (end_time.tv_sec - start_time.tv_sec) +
                         (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
    result->file_size_bytes = get_file_size(log_file);
    result->memory_kb = get_memory_usage();
    result->logs_per_sec = result->num_logs / result->elapsed_sec;
    result->mb_per_sec = (result->file_size_bytes / (1024.0 * 1024.0)) / result->elapsed_sec;
    result->compression_ratio = stats.compression_ratio_x100 / 100.0;
    result->dropped_logs = stats.dropped_logs;

    /* Calculate drop rate correctly */
    uint64_t total_attempted = stats.total_logs_written + stats.dropped_logs;
    result->drop_rate_percent = total_attempted > 0 ?
        (stats.dropped_logs * 100.0) / total_attempted : 0.0;

    /* Calculate latency percentiles */
    uint64_t min, p50, p95, p99, p999, max;
    calculate_percentiles(&min, &p50, &p95, &p99, &p999, &max);

    result->latency_min_ns = cycles_to_ns(min);
    result->latency_p50_ns = cycles_to_ns(p50);
    result->latency_p95_ns = cycles_to_ns(p95);
    result->latency_p99_ns = cycles_to_ns(p99);
    result->latency_p999_ns = cycles_to_ns(p999);
    result->latency_max_ns = cycles_to_ns(max);

    /* Calculate average latency */
    uint64_t sum = 0;
    for (size_t i = 0; i < g_num_samples; i++) {
        sum += g_latency_samples[i];
    }
    result->latency_avg_ns = g_num_samples > 0 ? cycles_to_ns(sum / g_num_samples) : 0;

    cnanolog_shutdown();

    /* Clean up */
    unlink(log_file);
}

/* ============================================================================
 * Multi-Threaded Benchmark
 * ============================================================================ */

typedef struct {
    int thread_id;
    uint64_t iterations;
    double start_time_sec;
    double end_time_sec;
    uint64_t dropped_before;
} mt_thread_args_t;

void* mt_worker_thread(void* arg) {
    mt_thread_args_t* args = (mt_thread_args_t*)arg;
    struct timespec ts;

    cnanolog_preallocate();

    clock_gettime(CLOCK_MONOTONIC, &ts);
    args->start_time_sec = ts.tv_sec + ts.tv_nsec / 1e9;

    for (uint64_t i = 0; i < args->iterations; i++) {
        LOG_INFO("Thread %d: iteration %d value %d",
                 args->thread_id, (int)i, (int)(i * args->thread_id));
    }

    clock_gettime(CLOCK_MONOTONIC, &ts);
    args->end_time_sec = ts.tv_sec + ts.tv_nsec / 1e9;

    return NULL;
}

void run_multithreaded_benchmark(int num_threads, uint64_t logs_per_thread,
                                benchmark_result_t* result, int cpu_affinity_core) {
    const char* log_file = "benchmark_mt_temp.clog";

    printf("  Testing multi-threaded (%d threads): %s logs per thread...\n",
           num_threads, format_number(logs_per_thread));

    if (cnanolog_init(log_file) != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return;
    }

    /* Set CPU affinity if requested */
    if (cpu_affinity_core >= 0) {
        if (cnanolog_set_writer_affinity(cpu_affinity_core) != 0) {
            fprintf(stderr, "Warning: Failed to set CPU affinity to core %d\n", cpu_affinity_core);
        }
    }

    cnanolog_preallocate();

    /* Create threads */
    cnanolog_thread_t* threads = malloc(num_threads * sizeof(cnanolog_thread_t));
    mt_thread_args_t* args = malloc(num_threads * sizeof(mt_thread_args_t));

    for (int i = 0; i < num_threads; i++) {
        args[i].thread_id = i;
        args[i].iterations = logs_per_thread;
    }

    /* Start benchmark */
    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    for (int i = 0; i < num_threads; i++) {
        cnanolog_thread_create(&threads[i], mt_worker_thread, &args[i]);
    }

    /* Wait for completion */
    for (int i = 0; i < num_threads; i++) {
        cnanolog_thread_join(threads[i], NULL);
    }

    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    /* Wait for background processing - longer for MT tests */
    struct timespec sleep_time = {2, 0};  /* 2 seconds */
    nanosleep(&sleep_time, NULL);

    /* Get statistics - multiple times to ensure flushing */
    cnanolog_stats_t stats;
    for (int i = 0; i < 3; i++) {
        cnanolog_get_stats(&stats);
        struct timespec short_wait = {0, 100000000};  /* 100ms */
        nanosleep(&short_wait, NULL);
    }

    /* Calculate results */
    uint64_t total_logs = num_threads * logs_per_thread;
    result->num_logs = total_logs;
    result->elapsed_sec = (end_time.tv_sec - start_time.tv_sec) +
                         (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
    result->file_size_bytes = get_file_size(log_file);
    result->memory_kb = get_memory_usage();
    result->logs_per_sec = total_logs / result->elapsed_sec;
    result->mb_per_sec = (result->file_size_bytes / (1024.0 * 1024.0)) / result->elapsed_sec;
    result->compression_ratio = stats.compression_ratio_x100 / 100.0;
    result->dropped_logs = stats.dropped_logs;

    /* Calculate drop rate correctly for multi-threaded */
    uint64_t total_attempted = stats.total_logs_written + stats.dropped_logs;
    result->drop_rate_percent = total_attempted > 0 ?
        (stats.dropped_logs * 100.0) / total_attempted : 0.0;

    free(threads);
    free(args);

    cnanolog_shutdown();
    unlink(log_file);
}

/* ============================================================================
 * Results Display
 * ============================================================================ */

void print_result_header(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                         BENCHMARK RESULTS                                    ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

void print_single_threaded_result(const char* scale_name, benchmark_result_t* result) {
    printf("─────────────────────────────────────────────────────────────────────────────\n");
    printf("  Scale: %s (%s logs)\n", scale_name, format_number(result->num_logs));
    printf("─────────────────────────────────────────────────────────────────────────────\n");
    printf("  Time elapsed:        %.3f seconds\n", result->elapsed_sec);
    printf("  File size:           %s\n", format_bytes(result->file_size_bytes));
    printf("  Memory usage:        %s\n", format_bytes(result->memory_kb * 1024));
    printf("\n");
    printf("  Throughput:\n");
    printf("    Logs/sec:          %.2f million\n", result->logs_per_sec / 1e6);
    printf("    MB/sec:            %.2f MB/s\n", result->mb_per_sec);
    printf("\n");
    printf("  Latency (per log call):\n");
    printf("    Min:               %.1f ns\n", result->latency_min_ns);
    printf("    p50 (median):      %.1f ns\n", result->latency_p50_ns);
    printf("    p95:               %.1f ns\n", result->latency_p95_ns);
    printf("    p99:               %.1f ns\n", result->latency_p99_ns);
    printf("    p99.9:             %.1f ns\n", result->latency_p999_ns);
    printf("    Max:               %.1f ns\n", result->latency_max_ns);
    printf("    Average:           %.1f ns\n", result->latency_avg_ns);
    printf("\n");
    printf("  Compression:         %.2fx\n", result->compression_ratio);
    printf("  Dropped logs:        %s (%.4f%%)\n",
           format_number(result->dropped_logs), result->drop_rate_percent);
    printf("\n");
}

void print_multithreaded_result(int num_threads, benchmark_result_t* result) {
    printf("─────────────────────────────────────────────────────────────────────────────\n");
    printf("  Multi-Threaded: %d threads (%s logs total)\n",
           num_threads, format_number(result->num_logs));
    printf("─────────────────────────────────────────────────────────────────────────────\n");
    printf("  Time elapsed:        %.3f seconds\n", result->elapsed_sec);
    printf("  File size:           %s\n", format_bytes(result->file_size_bytes));
    printf("\n");
    printf("  Aggregate throughput:\n");
    printf("    Total:             %.2f million logs/sec\n", result->logs_per_sec / 1e6);
    printf("    Per thread:        %.2f million logs/sec\n", (result->logs_per_sec / num_threads) / 1e6);
    printf("    MB/sec:            %.2f MB/s\n", result->mb_per_sec);
    printf("\n");
    printf("  Compression:         %.2fx\n", result->compression_ratio);
    printf("  Dropped logs:        %s (%.4f%%)\n",
           format_number(result->dropped_logs), result->drop_rate_percent);
    printf("\n");
}

/* ============================================================================
 * Main Benchmark Suite
 * ============================================================================ */

void print_usage(const char* prog) {
    printf("Usage: %s [cpu_core] [options]\n", prog);
    printf("\n");
    printf("Arguments:\n");
    printf("  cpu_core            CPU core number for writer thread affinity (optional)\n");
    printf("                      Use -1 or omit for no affinity (default)\n");
    printf("\n");
    printf("Options:\n");
    printf("  --extreme           Enable extreme scale test (10GB+)\n");
    printf("  --scale <name>      Run specific scale only\n");
    printf("  --multithreaded     Include multi-threaded tests\n");
    printf("  --threads <N>       Number of threads for MT tests (default: 4)\n");
    printf("  --help              Show this help\n");
    printf("\n");
    printf("Available scales:\n");
    for (int i = 0; i < g_num_scales; i++) {
        printf("  %-10s  %s logs (~%s)\n",
               g_scales[i].name,
               format_number(g_scales[i].num_logs),
               format_bytes(g_scales[i].num_logs * 50));  /* Rough estimate */
    }
    printf("\n");
    printf("Examples:\n");
    printf("  %s                  # Run without CPU affinity\n", prog);
    printf("  %s 7                # Pin writer thread to core 7\n", prog);
    printf("  %s 7 --extreme      # Pin to core 7 and run extreme tests\n", prog);
    printf("\n");
}

int main(int argc, char** argv) {
    int enable_multithreaded = 0;
    int num_threads = 4;
    const char* specific_scale = NULL;
    int cpu_affinity_core = -1;  /* -1 means no affinity */
    int arg_start = 1;

    /* Parse first positional argument as CPU core number */
    if (argc > 1 && argv[1][0] != '-') {
        cpu_affinity_core = atoi(argv[1]);
        arg_start = 2;  /* Start parsing flags from second argument */
    }

    /* Parse command line arguments */
    for (int i = arg_start; i < argc; i++) {
        if (strcmp(argv[i], "--extreme") == 0) {
            g_scales[g_num_scales - 1].enabled = 1;
        } else if (strcmp(argv[i], "--multithreaded") == 0) {
            enable_multithreaded = 1;
        } else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            num_threads = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--scale") == 0 && i + 1 < argc) {
            specific_scale = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    printf("╔══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║        CNanoLog Comprehensive Performance Benchmark                         ║\n");
    printf("║        Small to Extreme Scale (up to 10GB+)                                 ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    /* Display configuration */
    printf("Configuration:\n");
    if (cpu_affinity_core >= 0) {
        printf("  CPU Affinity: Writer thread pinned to core %d\n", cpu_affinity_core);
        printf("  Expected: 3x+ throughput improvement, near-zero drop rate\n");
    } else {
        printf("  CPU Affinity: Disabled (writer thread competes for CPU)\n");
        printf("  Note: Enable with './benchmark_comprehensive <core>' for best performance\n");
    }
    printf("\n");

    /* Calibrate CPU */
    printf("Calibrating CPU frequency...\n");
    calibrate_cpu_frequency();
    printf("  CPU Frequency: %.2f GHz\n", g_cpu_freq / 1e9);

    /* Allocate latency sample buffer */
    g_latency_samples = malloc(MAX_SAMPLES * sizeof(uint64_t));
    if (g_latency_samples == NULL) {
        fprintf(stderr, "Failed to allocate latency sample buffer\n");
        return 1;
    }

    printf("\n");
    printf("Starting benchmark...\n");

    /* Single-threaded benchmarks */
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("  SINGLE-THREADED BENCHMARKS\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n");

    for (int i = 0; i < g_num_scales; i++) {
        if (!g_scales[i].enabled) continue;

        if (specific_scale && strcmp(g_scales[i].name, specific_scale) != 0) {
            continue;
        }

        benchmark_result_t result;
        memset(&result, 0, sizeof(result));

        run_single_threaded_benchmark(&g_scales[i], &result, cpu_affinity_core);
        print_single_threaded_result(g_scales[i].name, &result);
    }

    /* Multi-threaded benchmarks */
    if (enable_multithreaded) {
        printf("\n");
        printf("═══════════════════════════════════════════════════════════════════════════════\n");
        printf("  MULTI-THREADED BENCHMARKS\n");
        printf("═══════════════════════════════════════════════════════════════════════════════\n");

        /* Test with medium scale for MT */
        benchmark_result_t result;

        run_multithreaded_benchmark(num_threads, 500000, &result, cpu_affinity_core);
        print_multithreaded_result(num_threads, &result);

        /* Test scaling with different thread counts */
        int thread_counts[] = {2, 4, 8};
        for (int i = 0; i < 3; i++) {
            if (thread_counts[i] > num_threads) break;

            run_multithreaded_benchmark(thread_counts[i], 200000, &result, cpu_affinity_core);
            print_multithreaded_result(thread_counts[i], &result);
        }
    }

    free(g_latency_samples);

    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                      BENCHMARK COMPLETE!                                     ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    return 0;
}
