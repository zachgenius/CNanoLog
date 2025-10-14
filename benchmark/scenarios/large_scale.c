/*
 * Large-Scale Benchmark Scenarios
 *
 * Tests performance with 100M+ logs (multi-GB scale).
 * Focuses on sustained throughput and reliability.
 */

#include "../common/benchmark_adapter.h"
#include "../common/timing.h"
#include "../common/stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* Forward declarations */
benchmark_adapter_t* get_cnanolog_adapter(void);

/* ============================================================================
 * Globals
 * ============================================================================ */

static uint64_t g_cpu_freq = 0;

/* ============================================================================
 * Large-Scale Test Configurations
 * ============================================================================ */

typedef struct {
    const char* name;
    uint64_t num_logs;
    uint64_t sample_interval;  /* Sample every N logs for latency */
    int report_interval_logs;  /* Report progress every N logs */
} scale_config_t;

static scale_config_t g_scales[] = {
    {"5M",      5000000,      1000, 1000000},
    {"10M",     10000000,     1000, 2000000},
    {"50M",     50000000,     2000, 10000000},
    {"100M",    100000000,    5000, 20000000},
    {"200M",    200000000,    5000, 40000000},
    {"500M",    500000000,    10000, 100000000},
    {"1B",      1000000000,   10000, 200000000},
};

/* ============================================================================
 * Large-Scale: Sustained Throughput Test
 * ============================================================================ */

typedef struct {
    const char* scale_name;
    uint64_t num_logs;
    double elapsed_sec;
    double throughput_mps;
    double latency_p50_ns;
    double latency_p99_ns;
    double latency_p999_ns;
    double latency_max_ns;
    double drop_rate;
    uint64_t total_drops;
    uint64_t file_size_mb;
} large_scale_result_t;

void run_large_scale_test(benchmark_adapter_t* adapter,
                          scale_config_t* scale,
                          large_scale_result_t* result) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════════════════╗\n");
    printf("║ Large-Scale Test: %s logs                                                ║\n", scale->name);
    printf("╚═══════════════════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    const uint64_t NUM_LOGS = scale->num_logs;
    const uint64_t WARMUP_LOGS = 10000;
    const uint64_t SAMPLE_INTERVAL = scale->sample_interval;

    /* Initialize adapter */
    bench_config_t config = {
        .use_timestamps = 1,
        .use_async = 1,
        .buffer_size_bytes = 8 * 1024 * 1024,  /* 8MB per thread */
        .num_threads = 1,
        .writer_cpu_affinity = -1,
        .flush_batch_size = 500,
        .flush_interval_ms = 50,
    };

    /* Auto-detect last CPU core */
    long num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cores > 0) {
        config.writer_cpu_affinity = (int)(num_cores - 1);
    }

    char log_file[256];
    snprintf(log_file, sizeof(log_file), "/tmp/bench_%s_large_%s.log",
             adapter->name, scale->name);

    printf("Initializing %s...\n", adapter->name);
    if (adapter->init(log_file, &config) != 0) {
        fprintf(stderr, "Failed to initialize %s\n", adapter->name);
        return;
    }

    adapter->thread_init();
    adapter->reset_stats();

    /* Create histogram for sampled latencies */
    uint64_t max_samples = NUM_LOGS / SAMPLE_INTERVAL + 1000;
    latency_histogram_t* hist = latency_histogram_create(max_samples);
    if (!hist) {
        fprintf(stderr, "Failed to allocate histogram\n");
        adapter->shutdown();
        return;
    }

    /* Warmup */
    printf("Warmup (%llu logs)...\n", (unsigned long long)WARMUP_LOGS);
    for (uint64_t i = 0; i < WARMUP_LOGS; i++) {
        adapter->log_2_ints("Warmup log %d: value=%d", (int)i, (int)(i * 2));
    }
    usleep(100000);  /* 100ms */
    adapter->reset_stats();

    /* Benchmark */
    printf("Running benchmark (%llu logs, sampling every %llu)...\n",
           (unsigned long long)NUM_LOGS,
           (unsigned long long)SAMPLE_INTERVAL);
    printf("─────────────────────────────────────────────────────────────────────────\n");

    uint64_t start_time = bench_get_time_ns();
    uint64_t last_report_time = start_time;
    uint64_t last_report_logs = 0;

    for (uint64_t i = 0; i < NUM_LOGS; i++) {
        /* Sample latency every SAMPLE_INTERVAL logs */
        if (i % SAMPLE_INTERVAL == 0) {
            uint64_t start = bench_rdtsc();
            adapter->log_2_ints("Benchmark log %llu: value=%llu",
                               (unsigned long long)i,
                               (unsigned long long)(i * 2));
            uint64_t end = bench_rdtsc();
            latency_histogram_add(hist, end - start);
        } else {
            adapter->log_2_ints("Benchmark log %llu: value=%llu",
                               (unsigned long long)i,
                               (unsigned long long)(i * 2));
        }

        /* Progress report */
        if (i > 0 && i % scale->report_interval_logs == 0) {
            uint64_t now = bench_get_time_ns();
            double interval_sec = (now - last_report_time) / 1e9;
            double interval_logs = i - last_report_logs;
            double interval_throughput = interval_logs / interval_sec / 1e6;

            bench_stats_t stats;
            adapter->get_stats(&stats);

            printf("  %llu / %llu logs (%.1f%%)  |  %.2f M/s  |  drops: %.4f%%\n",
                   (unsigned long long)i,
                   (unsigned long long)NUM_LOGS,
                   (i * 100.0) / NUM_LOGS,
                   interval_throughput,
                   stats.drop_rate_percent);

            last_report_time = now;
            last_report_logs = i;
        }
    }

    uint64_t end_time = bench_get_time_ns();
    double elapsed_sec = bench_elapsed_sec(start_time, end_time);

    printf("─────────────────────────────────────────────────────────────────────────\n");
    printf("Logging complete. Waiting for background writer to flush...\n");

    /* Wait for background processing - scale with log count */
    int wait_ms = 1000;  /* Base 1s */
    if (NUM_LOGS >= 100000000) wait_ms = 5000;   /* 100M+: 5s */
    if (NUM_LOGS >= 500000000) wait_ms = 10000;  /* 500M+: 10s */

    usleep(wait_ms * 1000);
    adapter->flush();

    /* Get final statistics */
    bench_stats_t stats;
    adapter->get_stats(&stats);

    /* Calculate latency percentiles from sampled data */
    summary_stats_t summary;
    latency_histogram_summary(hist, &summary);

    /* Store results */
    result->scale_name = scale->name;
    result->num_logs = NUM_LOGS;
    result->elapsed_sec = elapsed_sec;
    result->throughput_mps = NUM_LOGS / elapsed_sec / 1e6;
    result->latency_p50_ns = bench_cycles_to_ns(summary.p50, g_cpu_freq);
    result->latency_p99_ns = bench_cycles_to_ns(summary.p99, g_cpu_freq);
    result->latency_p999_ns = bench_cycles_to_ns(summary.p999, g_cpu_freq);
    result->latency_max_ns = bench_cycles_to_ns(summary.max, g_cpu_freq);
    result->drop_rate = stats.drop_rate_percent;
    result->total_drops = stats.total_drops;
    result->file_size_mb = stats.disk_writes_kb / 1024;

    /* Print results */
    printf("\n");
    printf("Results for %s:\n", adapter->name);
    printf("─────────────────────────────────────────────────────────────────────────\n");
    printf("  Scale:         %s logs\n", scale->name);
    printf("  Duration:      %.2f seconds\n", elapsed_sec);
    printf("\n");
    printf("  Throughput:\n");
    printf("    %.2f M logs/sec\n", result->throughput_mps);
    printf("    %.2f MB/sec\n", (result->file_size_mb / elapsed_sec));
    printf("\n");
    printf("  Latency (sampled):\n");
    printf("    p50:    %.1f ns\n", result->latency_p50_ns);
    printf("    p99:    %.1f ns\n", result->latency_p99_ns);
    printf("    p99.9:  %.1f ns\n", result->latency_p999_ns);
    printf("    max:    %.1f ns\n", result->latency_max_ns);
    printf("\n");
    printf("  Reliability:\n");
    printf("    Drop rate: %.4f%%\n", result->drop_rate);
    printf("    Dropped:   %llu / %llu\n",
           (unsigned long long)stats.total_drops,
           (unsigned long long)stats.total_logs_attempted);
    printf("\n");
    printf("  Resources:\n");
    printf("    File size:  %llu MB\n", (unsigned long long)result->file_size_mb);
    printf("    Memory:     %llu KB\n", (unsigned long long)stats.memory_rss_kb);
    printf("─────────────────────────────────────────────────────────────────────────\n");

    /* Cleanup */
    latency_histogram_destroy(hist);
    adapter->thread_cleanup();
    adapter->shutdown();
    unlink(log_file);
}

/* ============================================================================
 * Main
 * ============================================================================ */

void print_usage(const char* prog) {
    printf("Usage: %s [options]\n", prog);
    printf("\n");
    printf("Options:\n");
    printf("  --library <name>    Library to benchmark (default: cnanolog)\n");
    printf("  --scale <name>      Scale to test (default: 100M)\n");
    printf("  --help              Show this help\n");
    printf("\n");
    printf("Available libraries:\n");
    printf("  cnanolog            CNanoLog (our library)\n");
    printf("\n");
    printf("Available scales:\n");
    printf("  5M                  5 million logs (~25 MB)\n");
    printf("  10M                 10 million logs (~50 MB)\n");
    printf("  50M                 50 million logs (~250 MB)\n");
    printf("  100M                100 million logs (~500 MB)\n");
    printf("  200M                200 million logs (~1 GB)\n");
    printf("  500M                500 million logs (~2.5 GB)\n");
    printf("  1B                  1 billion logs (~5 GB)\n");
    printf("\n");
}

int main(int argc, char** argv) {
    const char* library = "cnanolog";
    const char* scale_name = "100M";

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--library") == 0 && i + 1 < argc) {
            library = argv[++i];
        } else if (strcmp(argv[i], "--scale") == 0 && i + 1 < argc) {
            scale_name = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    /* Calibrate CPU frequency */
    printf("Calibrating CPU frequency...\n");
    g_cpu_freq = bench_calibrate_cpu_frequency();
    printf("  CPU frequency: %.2f GHz\n", g_cpu_freq / 1e9);

    /* Get adapter */
    benchmark_adapter_t* adapter = NULL;

    if (strcmp(library, "cnanolog") == 0) {
        adapter = get_cnanolog_adapter();
    } else {
        fprintf(stderr, "Unknown library: %s\n", library);
        fprintf(stderr, "Only 'cnanolog' is implemented so far.\n");
        return 1;
    }

    /* Find scale configuration */
    scale_config_t* scale = NULL;
    for (size_t i = 0; i < sizeof(g_scales) / sizeof(g_scales[0]); i++) {
        if (strcmp(g_scales[i].name, scale_name) == 0) {
            scale = &g_scales[i];
            break;
        }
    }

    if (!scale) {
        fprintf(stderr, "Unknown scale: %s\n", scale_name);
        fprintf(stderr, "Available scales: 5M, 10M, 50M, 100M, 200M, 500M, 1B\n");
        return 1;
    }

    /* Run test */
    large_scale_result_t result;
    run_large_scale_test(adapter, scale, &result);

    /* Summary */
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════════════════╗\n");
    printf("║ Summary                                                                   ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("Library:    %s\n", adapter->name);
    printf("Scale:      %s logs (%llu)\n", scale->name, (unsigned long long)scale->num_logs);
    printf("Duration:   %.2f seconds\n", result.elapsed_sec);
    printf("Throughput: %.2f M logs/sec\n", result.throughput_mps);
    printf("Latency:    p50=%.1fns, p99=%.1fns, p99.9=%.1fns\n",
           result.latency_p50_ns, result.latency_p99_ns, result.latency_p999_ns);
    printf("Drop rate:  %.4f%%\n", result.drop_rate);
    printf("File size:  %llu MB\n", (unsigned long long)result.file_size_mb);
    printf("\n");

    if (result.drop_rate == 0.0) {
        printf("✅ EXCELLENT: 0%% drop rate\n");
    } else if (result.drop_rate < 0.1) {
        printf("✅ GOOD: Drop rate <0.1%%\n");
    } else if (result.drop_rate < 1.0) {
        printf("⚠️  WARNING: Drop rate <1%% but >0.1%%\n");
    } else {
        printf("❌ POOR: Drop rate >1%%\n");
    }
    printf("\n");

    return 0;
}
