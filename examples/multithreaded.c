/*
 * Multi-Threaded Logging Example
 *
 * Demonstrates:
 * - Thread-safe logging from multiple threads
 * - Using cnanolog_preallocate() for optimal performance
 * - Lock-free fast path
 * - Thread-local staging buffers
 */

#include <cnanolog.h>
#include "../src/platform.h"
#include <stdio.h>
#include <unistd.h>

#define NUM_WORKER_THREADS 4
#define LOGS_PER_THREAD 10000

typedef struct {
    int thread_id;
    int num_logs;
} worker_args_t;

/* Worker thread function */
void* worker_thread(void* arg) {
    worker_args_t* args = (worker_args_t*)arg;

    /* IMPORTANT: Preallocate staging buffer for this thread
     * This eliminates first-log allocation overhead (~292ns)
     */
    cnanolog_preallocate();

    log_info1("Worker thread %d started", args->thread_id);

    /* Simulate work with logging */
    for (int i = 0; i < args->num_logs; i++) {
        // Log at different levels
        if (i % 100 == 0) {
            log_info3("Worker %d: Progress %d/%d",
                     args->thread_id, i, args->num_logs);
        }

        if (i % 500 == 0) {
            log_debug2("Worker %d: Debug checkpoint at iteration %d",
                      args->thread_id, i);
        }

        // Simulate occasional warnings/errors
        if (i == args->num_logs / 2) {
            log_warn1("Worker %d: Halfway through processing",
                     args->thread_id);
        }
    }

    log_info2("Worker thread %d completed (%d logs written)",
             args->thread_id, args->num_logs);

    return NULL;
}

int main(void) {
    printf("=== CNanoLog Multi-Threaded Example ===\n\n");

    /* Initialize logger */
    if (cnanolog_init("multithreaded_example.clog") != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    printf("Logger initialized\n");
    printf("Creating %d worker threads...\n", NUM_WORKER_THREADS);
    printf("Each thread will log %d messages\n\n", LOGS_PER_THREAD);

    /* Preallocate for main thread */
    cnanolog_preallocate();

    log_info("Application started - multi-threaded logging test");

    /* Create worker threads */
    cnanolog_thread_t threads[NUM_WORKER_THREADS];
    worker_args_t args[NUM_WORKER_THREADS];

    for (int i = 0; i < NUM_WORKER_THREADS; i++) {
        args[i].thread_id = i;
        args[i].num_logs = LOGS_PER_THREAD;

        if (cnanolog_thread_create(&threads[i], worker_thread, &args[i]) != 0) {
            fprintf(stderr, "Failed to create thread %d\n", i);
            return 1;
        }

        log_info1("Created worker thread %d", i);
    }

    printf("All threads created\n");

    /* Main thread can also log while workers are running */
    for (int i = 0; i < 100; i++) {
        log_info1("Main thread: monitoring iteration %d", i);
        usleep(10000);  // 10ms sleep
    }

    /* Wait for all threads to complete */
    printf("Waiting for threads to complete...\n");

    for (int i = 0; i < NUM_WORKER_THREADS; i++) {
        cnanolog_thread_join(threads[i], NULL);
        printf("  Thread %d joined\n", i);
    }

    log_info("All worker threads completed");

    /* Give background thread time to process remaining logs */
    printf("\nWaiting for background thread to flush...\n");
    sleep(1);

    /* Get final statistics */
    cnanolog_stats_t stats;
    cnanolog_get_stats(&stats);

    printf("\n=== Statistics ===\n");
    printf("Total logs written:     %llu\n", (unsigned long long)stats.total_logs_written);
    printf("Total bytes written:    %llu bytes\n", (unsigned long long)stats.total_bytes_written);
    printf("Compression ratio:      %.2fx\n", stats.compression_ratio_x100 / 100.0);
    printf("Staging buffers active: %llu (one per thread)\n", (unsigned long long)stats.staging_buffers_active);
    printf("Dropped logs:           %llu\n", (unsigned long long)stats.dropped_logs);
    printf("Background wakeups:     %llu\n", (unsigned long long)stats.background_wakeups);

    double drop_rate = 0.0;
    if (stats.total_logs_written > 0) {
        drop_rate = (stats.dropped_logs * 100.0) /
                   (stats.total_logs_written + stats.dropped_logs);
    }
    printf("Drop rate:              %.2f%%\n", drop_rate);

    /* Shutdown */
    cnanolog_shutdown();
    printf("\n=== Example Complete ===\n");
    printf("To view logs, run:\n");
    printf("  ../tools/decompressor multithreaded_example.clog | head -50\n\n");

    return 0;
}
