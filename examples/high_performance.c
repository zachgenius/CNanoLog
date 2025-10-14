/*
 * High-Performance Logging Example
 *
 * Demonstrates:
 * - CPU affinity for 3x+ performance boost
 * - Preallocate API for predictable latency
 * - Optimizing for ultra-low latency (<20ns)
 * - Measuring and monitoring performance
 */

#include <cnanolog.h>
#include "../src/platform.h"
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#ifdef PLATFORM_LINUX
    #include <sched.h>
#elif defined(PLATFORM_WINDOWS)
    #include <windows.h>
#endif

/* Get number of CPU cores (platform-specific) */
int get_cpu_count(void) {
#ifdef PLATFORM_LINUX
    return sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(PLATFORM_MACOS)
    return sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(PLATFORM_WINDOWS)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
#else
    return 4;  // Default fallback
#endif
}

/* Benchmark function */
void benchmark_logging(const char* description, int num_logs) {
    struct timespec start, end;

    printf("\n%s\n", description);
    printf("Logging %d messages...\n", num_logs);

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < num_logs; i++) {
        log_info2("Benchmark message %d: value=%d", i, i * 2);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed = (end.tv_sec - start.tv_sec) +
                    (end.tv_nsec - start.tv_nsec) / 1e9;

    double logs_per_sec = num_logs / elapsed;
    double ns_per_log = (elapsed * 1e9) / num_logs;

    printf("  Time:        %.3f seconds\n", elapsed);
    printf("  Throughput:  %.2f M logs/sec\n", logs_per_sec / 1e6);
    printf("  Latency:     %.1f ns/log\n", ns_per_log);

    /* Get statistics */
    cnanolog_stats_t stats;
    cnanolog_get_stats(&stats);

    printf("  Dropped:     %llu logs\n", (unsigned long long)stats.dropped_logs);
}

int main(void) {
    printf("╔═══════════════════════════════════════════════════╗\n");
    printf("║   CNanoLog High-Performance Setup Example        ║\n");
    printf("╚═══════════════════════════════════════════════════╝\n\n");

    /* Detect system configuration */
    int num_cores = get_cpu_count();
    printf("System Configuration:\n");
    printf("  CPU cores:      %d\n", num_cores);
    printf("  Platform:       ");
#ifdef PLATFORM_LINUX
    printf("Linux\n");
#elif defined(PLATFORM_MACOS)
    printf("macOS\n");
#elif defined(PLATFORM_WINDOWS)
    printf("Windows\n");
#else
    printf("Unknown\n");
#endif
    printf("\n");

    /* Initialize logger */
    printf("Step 1: Initialize logger\n");
    if (cnanolog_init("high_performance_example.clog") != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }
    printf("  ✓ Logger initialized\n\n");

    /* Preallocate for main thread */
    printf("Step 2: Preallocate staging buffer\n");
    cnanolog_preallocate();
    printf("  ✓ Main thread buffer preallocated (~292ns saved on first log)\n\n");

    /* Set CPU affinity if we have enough cores */
    printf("Step 3: Configure CPU affinity\n");
    if (num_cores >= 4) {
        // Pin background writer to last core
        int target_core = num_cores - 1;
        printf("  Attempting to pin writer thread to core %d...\n", target_core);

        if (cnanolog_set_writer_affinity(target_core) == 0) {
            printf("  ✓ Writer thread pinned to core %d\n", target_core);
            printf("    Expected benefits:\n");
            printf("      - 3x+ throughput improvement\n");
            printf("      - Eliminates thread migration overhead\n");
            printf("      - Better cache locality\n");
            printf("      - Lower drop rate\n");
        } else {
            printf("  ⚠ Failed to set affinity (may not be supported)\n");
            printf("    Continuing without CPU affinity optimization\n");
        }
    } else {
        printf("  ⚠ Only %d cores available, skipping affinity\n", num_cores);
        printf("    (Recommended: 4+ cores for optimal performance)\n");
    }

    printf("\n╔═══════════════════════════════════════════════════╗\n");
    printf("║   Performance Benchmarks                          ║\n");
    printf("╚═══════════════════════════════════════════════════╝\n");

    /* Baseline benchmark */
    cnanolog_reset_stats();
    benchmark_logging("Benchmark 1: Baseline (10K logs)", 10000);

    /* Wait for processing */
    usleep(100000);  // 100ms

    /* High-volume benchmark */
    cnanolog_reset_stats();
    benchmark_logging("Benchmark 2: High-volume (100K logs)", 100000);

    /* Wait for processing */
    usleep(200000);  // 200ms

    /* Burst benchmark */
    cnanolog_reset_stats();
    benchmark_logging("Benchmark 3: Burst test (50K logs)", 50000);

    /* Wait for final processing */
    printf("\nWaiting for background thread to process...\n");
    sleep(1);

    /* Final statistics */
    printf("\n╔═══════════════════════════════════════════════════╗\n");
    printf("║   Final Statistics                                ║\n");
    printf("╚═══════════════════════════════════════════════════╝\n");

    cnanolog_stats_t stats;
    cnanolog_get_stats(&stats);

    printf("  Total logs written:     %llu\n", (unsigned long long)stats.total_logs_written);
    printf("  Total bytes written:    %llu bytes (%.2f MB)\n",
           (unsigned long long)stats.total_bytes_written,
           stats.total_bytes_written / (1024.0 * 1024.0));
    printf("  Compression ratio:      %.2fx\n", stats.compression_ratio_x100 / 100.0);
    printf("  Dropped logs:           %llu\n", (unsigned long long)stats.dropped_logs);
    printf("  Background wakeups:     %llu\n", (unsigned long long)stats.background_wakeups);

    if (stats.total_logs_written > 0) {
        double drop_rate = (stats.dropped_logs * 100.0) /
                          (stats.total_logs_written + stats.dropped_logs);
        printf("  Drop rate:              %.2f%%\n", drop_rate);

        if (drop_rate > 1.0) {
            printf("\n  ⚠ High drop rate detected!\n");
            printf("    Suggestions:\n");
            printf("      - Increase STAGING_BUFFER_SIZE\n");
            printf("      - Ensure CPU affinity is set\n");
            printf("      - Reduce logging frequency\n");
        } else {
            printf("\n  ✓ Drop rate is acceptable\n");
        }
    }

    /* Shutdown */
    printf("\n");
    cnanolog_shutdown();

    printf("╔═══════════════════════════════════════════════════╗\n");
    printf("║   Example Complete                                ║\n");
    printf("╚═══════════════════════════════════════════════════╝\n");

    printf("\nKey Takeaways:\n");
    printf("  1. Always call cnanolog_preallocate() in each thread\n");
    printf("  2. Set CPU affinity for 3x+ performance boost\n");
    printf("  3. Monitor drop rate and adjust buffer sizes\n");
    printf("  4. Target: <1%% drop rate for production\n");

    printf("\nTo view logs, run:\n");
    printf("  ../tools/decompressor high_performance_example.clog | less\n\n");

    return 0;
}
