/*
 * Statistics Monitoring Example
 *
 * Demonstrates:
 * - Real-time statistics monitoring
 * - Alerting on high drop rates
 * - Tracking throughput and compression
 * - Performance diagnostics
 */

#include <cnanolog.h>
#include "../src/platform.h"
#include <stdio.h>
#include <unistd.h>
#include <time.h>

/* Monitoring configuration */
#define MONITOR_INTERVAL_SEC 2
#define DROP_RATE_THRESHOLD 1.0    // Alert if > 1% drops
#define WARNING_THRESHOLD 5.0      // Warning if > 5% drops

/* Monitoring thread */
void* monitor_thread(void* arg) {
    (void)arg;

    cnanolog_preallocate();

    log_info("Monitoring thread started");

    cnanolog_stats_t prev_stats = {0};
    cnanolog_get_stats(&prev_stats);

    while (1) {
        sleep(MONITOR_INTERVAL_SEC);

        /* Get current statistics */
        cnanolog_stats_t stats;
        cnanolog_get_stats(&stats);

        /* Calculate deltas since last check */
        uint64_t logs_delta = stats.total_logs_written - prev_stats.total_logs_written;
        uint64_t bytes_delta = stats.total_bytes_written - prev_stats.total_bytes_written;
        uint64_t drops_delta = stats.dropped_logs - prev_stats.dropped_logs;

        /* Calculate rates */
        double logs_per_sec = logs_delta / (double)MONITOR_INTERVAL_SEC;
        double mb_per_sec = (bytes_delta / (1024.0 * 1024.0)) / MONITOR_INTERVAL_SEC;

        double drop_rate = 0.0;
        if (logs_delta > 0) {
            drop_rate = (drops_delta * 100.0) / (logs_delta + drops_delta);
        }

        /* Print monitoring report */
        printf("\n[MONITOR] Statistics Report:\n");
        printf("  Time:                  %d sec interval\n", MONITOR_INTERVAL_SEC);
        printf("  Logs written:          %llu (%.2f K/sec)\n",
               (unsigned long long)logs_delta, logs_per_sec / 1000.0);
        printf("  Bytes written:         %llu (%.2f MB/sec)\n",
               (unsigned long long)bytes_delta, mb_per_sec);
        printf("  Dropped logs:          %llu\n", (unsigned long long)drops_delta);
        printf("  Drop rate:             %.2f%%\n", drop_rate);
        printf("  Compression ratio:     %.2fx\n", stats.compression_ratio_x100 / 100.0);
        printf("  Staging buffers:       %llu\n", (unsigned long long)stats.staging_buffers_active);
        printf("  Background wakeups:    %llu\n", (unsigned long long)stats.background_wakeups);

        /* Check for alerts */
        if (drop_rate >= WARNING_THRESHOLD) {
            printf("  ⚠️  WARNING: High drop rate (%.2f%%)!\n", drop_rate);
            log_error1("High drop rate detected: %.2f%%", (int)(drop_rate * 100));

            printf("      Recommendations:\n");
            printf("        1. Increase STAGING_BUFFER_SIZE\n");
            printf("        2. Enable CPU affinity\n");
            printf("        3. Reduce logging frequency\n");
        } else if (drop_rate >= DROP_RATE_THRESHOLD) {
            printf("  ⚠️  ALERT: Moderate drop rate (%.2f%%)\n", drop_rate);
            log_warn1("Moderate drop rate: %.2f%%", (int)(drop_rate * 100));
        } else {
            printf("  ✓  Drop rate is healthy\n");
        }

        /* Check throughput */
        if (logs_per_sec > 1000000) {
            printf("  🚀 High throughput: %.2f M logs/sec\n", logs_per_sec / 1e6);
        }

        /* Save current stats for next iteration */
        prev_stats = stats;

        /* Check if we should exit (for demo purposes, exit after 5 reports) */
        static int report_count = 0;
        if (++report_count >= 5) {
            log_info("Monitoring thread stopping after 5 reports");
            break;
        }
    }

    log_info("Monitoring thread stopped");
    return NULL;
}

/* Worker thread that generates logs */
void* worker_thread(void* arg) {
    int thread_id = *(int*)arg;

    cnanolog_preallocate();

    log_info1("Worker %d started", thread_id);

    /* Generate logs at varying rates */
    for (int burst = 0; burst < 10; burst++) {
        /* Log burst */
        for (int i = 0; i < 5000; i++) {
            log_info3("Worker %d: burst %d, iteration %d", thread_id, burst, i);
        }

        /* Vary the logging rate */
        usleep(100000 * (burst % 3));  // 0-200ms pause
    }

    log_info1("Worker %d completed", thread_id);
    return NULL;
}

int main(void) {
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║   CNanoLog Statistics Monitoring Example            ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    /* Initialize logger */
    if (cnanolog_init("monitoring_example.clog") != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    printf("Logger initialized\n");
    printf("Starting monitoring and worker threads...\n\n");

    cnanolog_preallocate();

    log_info("Application started - statistics monitoring demo");

    /* Start monitoring thread */
    cnanolog_thread_t monitor_tid;
    if (cnanolog_thread_create(&monitor_tid, monitor_thread, NULL) != 0) {
        fprintf(stderr, "Failed to create monitoring thread\n");
        return 1;
    }

    printf("Monitoring thread started (reports every %d seconds)\n", MONITOR_INTERVAL_SEC);

    /* Start worker threads */
    const int num_workers = 3;
    cnanolog_thread_t worker_tids[num_workers];
    int worker_ids[num_workers];

    for (int i = 0; i < num_workers; i++) {
        worker_ids[i] = i;
        if (cnanolog_thread_create(&worker_tids[i], worker_thread, &worker_ids[i]) != 0) {
            fprintf(stderr, "Failed to create worker thread %d\n", i);
            return 1;
        }
    }

    printf("Started %d worker threads\n", num_workers);
    printf("\nWatch the monitoring reports below:\n");
    printf("═══════════════════════════════════════════════════════\n");

    /* Wait for workers to complete */
    for (int i = 0; i < num_workers; i++) {
        cnanolog_thread_join(worker_tids[i], NULL);
    }

    printf("\nAll workers completed\n");

    /* Wait for monitor thread */
    cnanolog_thread_join(monitor_tid, NULL);

    printf("\nMonitoring complete\n");

    /* Final summary */
    printf("\n╔══════════════════════════════════════════════════════╗\n");
    printf("║   Final Summary                                      ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");

    cnanolog_stats_t final_stats;
    cnanolog_get_stats(&final_stats);

    printf("  Total logs written:     %llu\n", (unsigned long long)final_stats.total_logs_written);
    printf("  Total bytes written:    %llu bytes (%.2f MB)\n",
           (unsigned long long)final_stats.total_bytes_written,
           final_stats.total_bytes_written / (1024.0 * 1024.0));
    printf("  Total dropped logs:     %llu\n", (unsigned long long)final_stats.dropped_logs);
    printf("  Compression ratio:      %.2fx\n", final_stats.compression_ratio_x100 / 100.0);
    printf("  Staging buffers used:   %llu\n", (unsigned long long)final_stats.staging_buffers_active);

    double overall_drop_rate = 0.0;
    if (final_stats.total_logs_written > 0) {
        overall_drop_rate = (final_stats.dropped_logs * 100.0) /
                           (final_stats.total_logs_written + final_stats.dropped_logs);
    }
    printf("  Overall drop rate:      %.2f%%\n", overall_drop_rate);

    if (overall_drop_rate < 1.0) {
        printf("\n  ✓ Excellent: Drop rate is under 1%%\n");
    } else if (overall_drop_rate < 5.0) {
        printf("\n  ⚠ Good: Drop rate is acceptable but could be improved\n");
    } else {
        printf("\n  ⚠️  Warning: Drop rate is high, consider optimizations\n");
    }

    /* Shutdown */
    cnanolog_shutdown();

    printf("\n╔══════════════════════════════════════════════════════╗\n");
    printf("║   Example Complete                                   ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");

    printf("\nKey Takeaways:\n");
    printf("  1. Monitor statistics periodically in a dedicated thread\n");
    printf("  2. Set alert thresholds for drop rates\n");
    printf("  3. Track throughput to detect anomalies\n");
    printf("  4. Use monitoring data to tune buffer sizes\n");

    printf("\nTo view logs, run:\n");
    printf("  ../tools/decompressor monitoring_example.clog | grep -E '(ERROR|WARNING|started|stopped)'\n\n");

    return 0;
}
