/*
 * Single-Threaded Benchmark Scenarios
 *
 * Implements ST-1 through ST-4 from the testing plan.
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
 * ST-1: Baseline Latency
 * ============================================================================ */

typedef struct {
    const char* name;
    double latency_p50_ns;
    double latency_p99_ns;
    double latency_p999_ns;
    double latency_max_ns;
    double throughput_mps;
    double drop_rate;
} st1_result_t;

void run_st1_baseline_latency(benchmark_adapter_t* adapter, st1_result_t* result) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════════════════╗\n");
    printf("║ ST-1: Baseline Latency (1M logs, single-threaded)                        ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    const uint64_t NUM_LOGS = 1000000;
    const uint64_t WARMUP_LOGS = 10000;

    /* Initialize adapter */
    bench_config_t config = {
        .use_timestamps = 1,
        .use_async = 1,
        .buffer_size_bytes = 8 * 1024 * 1024,
        .num_threads = 1,
        .writer_cpu_affinity = -1,  /* Set based on system */
        .flush_batch_size = 500,
        .flush_interval_ms = 50,
    };

    /* Auto-detect last CPU core */
    long num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cores > 0) {
        config.writer_cpu_affinity = (int)(num_cores - 1);
    }

    char log_file[256];
    snprintf(log_file, sizeof(log_file), "/tmp/bench_%s_st1.log",
             adapter->name);

    printf("Initializing %s...\n", adapter->name);
    if (adapter->init(log_file, &config) != 0) {
        fprintf(stderr, "Failed to initialize %s\n", adapter->name);
        return;
    }

    adapter->thread_init();
    adapter->reset_stats();

    /* Warmup */
    printf("Warmup (%llu logs)...\n", (unsigned long long)WARMUP_LOGS);
    for (uint64_t i = 0; i < WARMUP_LOGS; i++) {
        adapter->log_2_ints("Warmup log %d: value=%d", (int)i, (int)(i * 2));
    }
    usleep(100000);  /* 100ms */
    adapter->reset_stats();

    /* Create latency histogram */
    latency_histogram_t* hist = latency_histogram_create(NUM_LOGS);
    if (!hist) {
        fprintf(stderr, "Failed to allocate histogram\n");
        adapter->shutdown();
        return;
    }

    /* Benchmark */
    printf("Running benchmark (%llu logs)...\n", (unsigned long long)NUM_LOGS);

    uint64_t start_time = bench_get_time_ns();

    for (uint64_t i = 0; i < NUM_LOGS; i++) {
        uint64_t start = bench_rdtsc();
        adapter->log_2_ints("Benchmark log %d: value=%d", (int)i, (int)(i * 2));
        uint64_t end = bench_rdtsc();

        latency_histogram_add(hist, end - start);
    }

    uint64_t end_time = bench_get_time_ns();

    /* Wait for background processing */
    printf("Waiting for background writer...\n");
    usleep(500000);  /* 500ms */
    adapter->flush();

    /* Get statistics */
    bench_stats_t stats;
    adapter->get_stats(&stats);

    /* Calculate latency percentiles */
    summary_stats_t summary;
    latency_histogram_summary(hist, &summary);

    /* Convert to nanoseconds */
    double elapsed_sec = bench_elapsed_sec(start_time, end_time);
    double throughput = NUM_LOGS / elapsed_sec;

    /* Store results */
    result->name = adapter->name;
    result->latency_p50_ns = bench_cycles_to_ns(summary.p50, g_cpu_freq);
    result->latency_p99_ns = bench_cycles_to_ns(summary.p99, g_cpu_freq);
    result->latency_p999_ns = bench_cycles_to_ns(summary.p999, g_cpu_freq);
    result->latency_max_ns = bench_cycles_to_ns(summary.max, g_cpu_freq);
    result->throughput_mps = throughput / 1e6;
    result->drop_rate = stats.drop_rate_percent;

    /* Print results */
    printf("\n");
    printf("Results for %s:\n", adapter->name);
    printf("─────────────────────────────────────────────────────────────────────────\n");
    printf("  Latency:\n");
    printf("    p50:    %.1f ns\n", result->latency_p50_ns);
    printf("    p99:    %.1f ns\n", result->latency_p99_ns);
    printf("    p99.9:  %.1f ns\n", result->latency_p999_ns);
    printf("    max:    %.1f ns\n", result->latency_max_ns);
    printf("\n");
    printf("  Throughput:\n");
    printf("    %.2f M logs/sec\n", result->throughput_mps);
    printf("\n");
    printf("  Reliability:\n");
    printf("    Drop rate: %.4f%%\n", result->drop_rate);
    printf("    Dropped: %llu / %llu\n",
           (unsigned long long)stats.total_drops,
           (unsigned long long)stats.total_logs_attempted);
    printf("\n");
    printf("  Resources:\n");
    printf("    Memory: %llu KB\n", (unsigned long long)stats.memory_rss_kb);
    printf("    Disk:   %llu KB\n", (unsigned long long)stats.disk_writes_kb);
    printf("─────────────────────────────────────────────────────────────────────────\n");

    /* Cleanup */
    latency_histogram_destroy(hist);
    adapter->thread_cleanup();
    adapter->shutdown();
    unlink(log_file);
}

/* ============================================================================
 * ST-4: Variable Data Types
 * ============================================================================ */

typedef struct {
    const char* type_name;
    double latency_p50_ns;
    double latency_p99_ns;
    double latency_max_ns;
    double throughput_mps;
} st4_type_result_t;

typedef struct {
    const char* name;
    st4_type_result_t results[11];  /* One per data type */
    int num_types;
} st4_result_t;

void run_st4_data_types(benchmark_adapter_t* adapter, st4_result_t* result) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════════════════╗\n");
    printf("║ ST-4: Variable Data Types (100K logs per type, single-threaded)          ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    const uint64_t NUM_LOGS = 100000;
    const uint64_t WARMUP_LOGS = 1000;

    /* Initialize adapter */
    bench_config_t config = {
        .use_timestamps = 1,
        .use_async = 1,
        .buffer_size_bytes = 8 * 1024 * 1024,
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
    snprintf(log_file, sizeof(log_file), "/tmp/bench_%s_st4.log", adapter->name);

    printf("Initializing %s...\n", adapter->name);
    if (adapter->init(log_file, &config) != 0) {
        fprintf(stderr, "Failed to initialize %s\n", adapter->name);
        return;
    }

    adapter->thread_init();
    adapter->reset_stats();

    /* Store results */
    result->name = adapter->name;
    result->num_types = 0;

    /* Test each data type */
    const char* type_names[] = {
        "1 int",
        "2 ints",
        "4 ints",
        "8 ints",
        "1 long",
        "1 uint64",
        "1 float",
        "1 double",
        "1 string",
        "mixed (int+string+int)",
        "mixed2 (int+double+string)"
    };

    printf("\n");
    printf("Testing different data types (%llu logs per type)...\n", (unsigned long long)NUM_LOGS);
    printf("─────────────────────────────────────────────────────────────────────────\n");

    for (int type_idx = 0; type_idx < 11; type_idx++) {
        latency_histogram_t* hist = latency_histogram_create(NUM_LOGS);
        if (!hist) {
            fprintf(stderr, "Failed to allocate histogram\n");
            continue;
        }

        /* Warmup */
        for (uint64_t i = 0; i < WARMUP_LOGS; i++) {
            switch (type_idx) {
                case 0: adapter->log_1_int("Warmup: %d", (int)i); break;
                case 1: adapter->log_2_ints("Warmup: %d %d", (int)i, (int)(i*2)); break;
                case 2: adapter->log_4_ints("Warmup: %d %d %d %d", (int)i, (int)(i*2), (int)(i*3), (int)(i*4)); break;
                case 3: adapter->log_8_ints("Warmup: %d %d %d %d %d %d %d %d", (int)i, (int)(i*2), (int)(i*3), (int)(i*4), (int)(i*5), (int)(i*6), (int)(i*7), (int)(i*8)); break;
                case 4: adapter->log_1_long("Warmup: %ld", (long)i); break;
                case 5: adapter->log_1_uint64("Warmup: %llu", (uint64_t)i); break;
                case 6: adapter->log_1_float("Warmup: %f", (float)i * 1.5f); break;
                case 7: adapter->log_1_double("Warmup: %f", (double)i * 1.5); break;
                case 8: adapter->log_1_string("Warmup: %s", "test_string"); break;
                case 9: adapter->log_mixed("Warmup: %d %s %d", (int)i, "mid", (int)(i*2)); break;
                case 10: adapter->log_mixed2("Warmup: %d %f %s", (int)i, (double)i * 1.5, "end"); break;
            }
        }
        usleep(10000);  /* 10ms */
        adapter->reset_stats();

        /* Benchmark this type */
        uint64_t start_time = bench_get_time_ns();

        for (uint64_t i = 0; i < NUM_LOGS; i++) {
            uint64_t start = bench_rdtsc();

            switch (type_idx) {
                case 0: adapter->log_1_int("Benchmark: %d", (int)i); break;
                case 1: adapter->log_2_ints("Benchmark: %d %d", (int)i, (int)(i*2)); break;
                case 2: adapter->log_4_ints("Benchmark: %d %d %d %d", (int)i, (int)(i*2), (int)(i*3), (int)(i*4)); break;
                case 3: adapter->log_8_ints("Benchmark: %d %d %d %d %d %d %d %d", (int)i, (int)(i*2), (int)(i*3), (int)(i*4), (int)(i*5), (int)(i*6), (int)(i*7), (int)(i*8)); break;
                case 4: adapter->log_1_long("Benchmark: %ld", (long)i); break;
                case 5: adapter->log_1_uint64("Benchmark: %llu", (uint64_t)i); break;
                case 6: adapter->log_1_float("Benchmark: %f", (float)i * 1.5f); break;
                case 7: adapter->log_1_double("Benchmark: %f", (double)i * 1.5); break;
                case 8: adapter->log_1_string("Benchmark: %s", "test_string_with_some_length"); break;
                case 9: adapter->log_mixed("Benchmark: %d %s %d", (int)i, "middle_string", (int)(i*2)); break;
                case 10: adapter->log_mixed2("Benchmark: %d %f %s", (int)i, (double)i * 1.5, "end_string"); break;
            }

            uint64_t end = bench_rdtsc();
            latency_histogram_add(hist, end - start);
        }

        uint64_t end_time = bench_get_time_ns();

        /* Calculate statistics */
        summary_stats_t summary;
        latency_histogram_summary(hist, &summary);

        double elapsed_sec = bench_elapsed_sec(start_time, end_time);
        double throughput = NUM_LOGS / elapsed_sec;

        /* Store result */
        st4_type_result_t* tres = &result->results[result->num_types++];
        tres->type_name = type_names[type_idx];
        tres->latency_p50_ns = bench_cycles_to_ns(summary.p50, g_cpu_freq);
        tres->latency_p99_ns = bench_cycles_to_ns(summary.p99, g_cpu_freq);
        tres->latency_max_ns = bench_cycles_to_ns(summary.max, g_cpu_freq);
        tres->throughput_mps = throughput / 1e6;

        /* Print result */
        printf("  %-28s  p50: %7.1f ns  p99: %8.1f ns  max: %10.1f ns  %.2f M/s\n",
               tres->type_name,
               tres->latency_p50_ns,
               tres->latency_p99_ns,
               tres->latency_max_ns,
               tres->throughput_mps);

        latency_histogram_destroy(hist);
    }

    /* Wait for background processing */
    printf("\nWaiting for background writer...\n");
    usleep(500000);  /* 500ms */
    adapter->flush();

    /* Get final statistics */
    bench_stats_t stats;
    adapter->get_stats(&stats);

    printf("\n");
    printf("Overall Statistics:\n");
    printf("─────────────────────────────────────────────────────────────────────────\n");
    printf("  Total logs:     %llu\n", (unsigned long long)stats.total_logs_written);
    printf("  Drop rate:      %.4f%%\n", stats.drop_rate_percent);
    printf("  Dropped:        %llu / %llu\n",
           (unsigned long long)stats.total_drops,
           (unsigned long long)stats.total_logs_attempted);
    printf("  Memory:         %llu KB\n", (unsigned long long)stats.memory_rss_kb);
    printf("  Disk:           %llu KB\n", (unsigned long long)stats.disk_writes_kb);
    printf("─────────────────────────────────────────────────────────────────────────\n");

    /* Cleanup */
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
    printf("  --scenario <name>   Scenario to run (default: ST-1)\n");
    printf("  --help              Show this help\n");
    printf("\n");
    printf("Available libraries:\n");
    printf("  cnanolog            CNanoLog (our library)\n");
    printf("  nanolog             NanoLog (reference implementation)\n");
    printf("  spdlog              spdlog\n");
    printf("  boost               Boost.Log\n");
    printf("  glog                Google glog\n");
    printf("\n");
    printf("Available scenarios:\n");
    printf("  ST-1                Baseline latency (1M logs)\n");
    printf("  ST-2                Sustained throughput (10 seconds)\n");
    printf("  ST-3                Burst performance (10 bursts)\n");
    printf("  ST-4                Variable message sizes\n");
    printf("\n");
}

int main(int argc, char** argv) {
    const char* library = "cnanolog";
    const char* scenario = "ST-1";

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--library") == 0 && i + 1 < argc) {
            library = argv[++i];
        } else if (strcmp(argv[i], "--scenario") == 0 && i + 1 < argc) {
            scenario = argv[++i];
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

    /* Run scenario */
    if (strcmp(scenario, "ST-1") == 0) {
        st1_result_t result;
        run_st1_baseline_latency(adapter, &result);

        /* Summary */
        printf("\n");
        printf("╔═══════════════════════════════════════════════════════════════════════════╗\n");
        printf("║ Summary                                                                   ║\n");
        printf("╚═══════════════════════════════════════════════════════════════════════════╝\n");
        printf("\n");
        printf("Library:    %s\n", result.name);
        printf("Scenario:   ST-1 (Baseline Latency)\n");
        printf("Result:     p50=%.1fns, p99=%.1fns, %.2f M logs/sec, %.4f%% drops\n",
               result.latency_p50_ns, result.latency_p99_ns,
               result.throughput_mps, result.drop_rate);
        printf("\n");

        if (result.drop_rate == 0.0) {
            printf("✅ EXCELLENT: 0%% drop rate\n");
        } else if (result.drop_rate < 1.0) {
            printf("✅ GOOD: Drop rate <1%%\n");
        } else {
            printf("⚠️  WARNING: Drop rate >1%%\n");
        }
        printf("\n");

    } else if (strcmp(scenario, "ST-4") == 0) {
        st4_result_t result;
        run_st4_data_types(adapter, &result);

        /* Summary */
        printf("\n");
        printf("╔═══════════════════════════════════════════════════════════════════════════╗\n");
        printf("║ Summary: Data Type Performance Comparison                                ║\n");
        printf("╚═══════════════════════════════════════════════════════════════════════════╝\n");
        printf("\n");
        printf("Library:    %s\n", result.name);
        printf("Scenario:   ST-4 (Variable Data Types)\n");
        printf("\n");
        printf("Performance across %d data types:\n", result.num_types);
        printf("─────────────────────────────────────────────────────────────────────────\n");

        /* Find fastest and slowest */
        double fastest_p50 = result.results[0].latency_p50_ns;
        double slowest_p50 = result.results[0].latency_p50_ns;
        const char* fastest_name = result.results[0].type_name;
        const char* slowest_name = result.results[0].type_name;

        for (int i = 1; i < result.num_types; i++) {
            if (result.results[i].latency_p50_ns < fastest_p50) {
                fastest_p50 = result.results[i].latency_p50_ns;
                fastest_name = result.results[i].type_name;
            }
            if (result.results[i].latency_p50_ns > slowest_p50) {
                slowest_p50 = result.results[i].latency_p50_ns;
                slowest_name = result.results[i].type_name;
            }
        }

        printf("  Fastest:  %s at %.1f ns (p50)\n", fastest_name, fastest_p50);
        printf("  Slowest:  %s at %.1f ns (p50)\n", slowest_name, slowest_p50);
        printf("  Range:    %.1f ns (%.1fx)\n",
               slowest_p50 - fastest_p50,
               slowest_p50 / fastest_p50);
        printf("\n");

    } else {
        fprintf(stderr, "Unknown scenario: %s\n", scenario);
        fprintf(stderr, "Available scenarios: ST-1, ST-4\n");
        return 1;
    }

    return 0;
}
