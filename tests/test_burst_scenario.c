/*
 * CNanoLog Burst Scenario Test
 *
 * Simulates realistic burst logging pattern:
 * - Normal rate: 100K logs/sec
 * - Burst spike: 10M logs in short time
 * - Back to normal
 *
 * This is different from benchmark_comprehensive which runs at maximum
 * sustained rate (unrealistic).
 */

#include <cnanolog.h>
#include "../src/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

/* Simulate realistic burst pattern */
void test_burst_pattern(int cpu_core) {
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║          CNanoLog Realistic Burst Scenario Test             ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    if (cnanolog_init("burst_test.clog") != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return;
    }

    /* Set CPU affinity if specified */
    if (cpu_core >= 0) {
        printf("CPU Affinity: Pinning writer to core %d\n", cpu_core);
        if (cnanolog_set_writer_affinity(cpu_core) != 0) {
            printf("Warning: Failed to set affinity\n");
        }
    } else {
        printf("CPU Affinity: Disabled\n");
    }

    cnanolog_preallocate();

    printf("\n");
    printf("Simulating realistic application pattern:\n");
    printf("  Phase 1: Normal logging (100K logs/sec for 1 second)\n");
    printf("  Phase 2: BURST (5M logs as fast as possible)\n");
    printf("  Phase 3: Normal logging (100K logs/sec for 1 second)\n");
    printf("\n");

    cnanolog_reset_stats();

    /* Phase 1: Normal rate - 100K logs/sec for 1 second */
    printf("Phase 1: Normal rate logging...\n");
    struct timespec sleep_time = {0, 10000};  /* 10 microseconds between logs */
    for (int i = 0; i < 100000; i++) {
        LOG_INFO("Normal operation log %d: status=%d", i, 200);
        nanosleep(&sleep_time, NULL);  /* Pace the logging */
    }

    cnanolog_stats_t stats_phase1;
    cnanolog_get_stats(&stats_phase1);
    printf("  Completed: %llu logs, dropped: %llu (%.2f%%)\n",
           (unsigned long long)stats_phase1.total_logs_written,
           (unsigned long long)stats_phase1.dropped_logs,
           stats_phase1.total_logs_written > 0 ?
               (stats_phase1.dropped_logs * 100.0) / stats_phase1.total_logs_written : 0.0);

    /* Phase 2: BURST - 5M logs as fast as possible */
    printf("\nPhase 2: BURST logging (5M logs, no pacing)...\n");
    struct timespec burst_start, burst_end;
    clock_gettime(CLOCK_MONOTONIC, &burst_start);

    for (int i = 0; i < 5000000; i++) {
        LOG_INFO("Burst log %d: value=%d", i, i * 2);
    }

    clock_gettime(CLOCK_MONOTONIC, &burst_end);
    double burst_duration = (burst_end.tv_sec - burst_start.tv_sec) +
                           (burst_end.tv_nsec - burst_start.tv_nsec) / 1e9;

    /* Wait for background writer to catch up */
    sleep(2);

    cnanolog_stats_t stats_phase2;
    cnanolog_get_stats(&stats_phase2);
    uint64_t burst_written = stats_phase2.total_logs_written - stats_phase1.total_logs_written;
    uint64_t burst_dropped = stats_phase2.dropped_logs - stats_phase1.dropped_logs;

    printf("  Burst duration: %.3f seconds\n", burst_duration);
    printf("  Burst rate: %.2f M logs/sec\n", 5.0 / burst_duration);
    printf("  Burst completed: %llu logs, dropped: %llu (%.2f%%)\n",
           (unsigned long long)burst_written,
           (unsigned long long)burst_dropped,
           burst_written > 0 ? (burst_dropped * 100.0) / (burst_written + burst_dropped) : 0.0);

    /* Phase 3: Back to normal - 100K logs/sec for 1 second */
    printf("\nPhase 3: Back to normal rate...\n");
    for (int i = 0; i < 100000; i++) {
        LOG_INFO("Post-burst log %d: status=%d", i, 200);
        nanosleep(&sleep_time, NULL);
    }

    /* Wait for final processing */
    sleep(1);

    cnanolog_stats_t stats_final;
    cnanolog_get_stats(&stats_final);
    uint64_t phase3_written = stats_final.total_logs_written - stats_phase2.total_logs_written;
    uint64_t phase3_dropped = stats_final.dropped_logs - stats_phase2.dropped_logs;

    printf("  Completed: %llu logs, dropped: %llu (%.2f%%)\n",
           (unsigned long long)phase3_written,
           (unsigned long long)phase3_dropped,
           phase3_written > 0 ? (phase3_dropped * 100.0) / phase3_written : 0.0);

    /* Final summary */
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                      FINAL SUMMARY                           ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");

    uint64_t total_attempted = stats_final.total_logs_written + stats_final.dropped_logs;
    double overall_drop_rate = total_attempted > 0 ?
        (stats_final.dropped_logs * 100.0) / total_attempted : 0.0;

    printf("\n");
    printf("Total logs attempted:  %llu\n", (unsigned long long)total_attempted);
    printf("Total logs written:    %llu\n", (unsigned long long)stats_final.total_logs_written);
    printf("Total logs dropped:    %llu\n", (unsigned long long)stats_final.dropped_logs);
    printf("Overall drop rate:     %.2f%%\n", overall_drop_rate);
    printf("\n");
    printf("File size:             %llu bytes (%.2f MB)\n",
           (unsigned long long)stats_final.total_bytes_written,
           stats_final.total_bytes_written / (1024.0 * 1024.0));
    printf("Compression ratio:     %.2fx\n", stats_final.compression_ratio_x100 / 100.0);
    printf("\n");

    /* Interpretation */
    if (overall_drop_rate < 1.0) {
        printf("✅ EXCELLENT: Drop rate <1%% - buffers handled burst perfectly!\n");
    } else if (overall_drop_rate < 5.0) {
        printf("✅ GOOD: Drop rate <5%% - acceptable for burst scenarios\n");
    } else if (overall_drop_rate < 15.0) {
        printf("⚠️  MODERATE: Drop rate <15%% - consider 8MB buffers\n");
    } else {
        printf("❌ HIGH: Drop rate >15%% - need larger buffers or CPU affinity\n");
    }

    printf("\n");
    printf("Key insight: Normal phases should have 0%% drops.\n");
    printf("             Burst phase drops are expected if burst is extreme.\n");
    printf("             Overall drop rate matters for your SLA.\n");
    printf("\n");

    cnanolog_shutdown();
    unlink("burst_test.clog");
}

int main(int argc, char** argv) {
    int cpu_core = -1;

    /* Parse CPU core from first argument */
    if (argc > 1) {
        cpu_core = atoi(argv[1]);
    }

    test_burst_pattern(cpu_core);

    return 0;
}
