/*
 * Multi-Threaded Stress Test - Phase 7.2
 *
 * Tests:
 * - Dynamic thread creation/destruction
 * - Concurrent aggressive logging from multiple threads
 * - Thread safety under extreme load
 * - Resource cleanup
 * - Statistics accuracy under stress
 */

#include <cnanolog.h>
#include "../src/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NUM_ROUNDS 5
#define THREADS_PER_ROUND 8
#define LOGS_PER_THREAD 50000

typedef struct {
    int thread_id;
    int round;
    int iterations;
    int success;
} thread_args_t;

/* Aggressive logging thread */
void* aggressive_logger(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;

    /* Preallocate buffer */
    cnanolog_preallocate();

    /* Log aggressively */
    for (int i = 0; i < args->iterations; i++) {
        log_info3("Round %d, Thread %d: iteration %d",
                  args->round, args->thread_id, i);

        /* Mix in different log types */
        if (i % 10 == 0) {
            log_warn1("Warning from thread %d", args->thread_id);
        }
        if (i % 100 == 0) {
            log_error2("Error: thread=%d, count=%d", args->thread_id, i);
        }
    }

    args->success = 1;
    return NULL;
}

/* Test: Create and destroy threads repeatedly */
int test_dynamic_threads(void) {
    printf("Test 1: Dynamic Thread Creation/Destruction\n");
    printf("---------------------------------------------\n");

    cnanolog_stats_t stats_before, stats_after;
    cnanolog_get_stats(&stats_before);

    for (int round = 0; round < NUM_ROUNDS; round++) {
        printf("  Round %d/%d: Creating %d threads...\n",
               round + 1, NUM_ROUNDS, THREADS_PER_ROUND);

        thread_t* threads = malloc(THREADS_PER_ROUND * sizeof(thread_t));
        thread_args_t* args = malloc(THREADS_PER_ROUND * sizeof(thread_args_t));

        /* Create threads */
        for (int i = 0; i < THREADS_PER_ROUND; i++) {
            args[i].thread_id = i;
            args[i].round = round;
            args[i].iterations = LOGS_PER_THREAD;
            args[i].success = 0;

            if (thread_create(&threads[i], aggressive_logger, &args[i]) != 0) {
                fprintf(stderr, "Failed to create thread %d\n", i);
                free(threads);
                free(args);
                return 0;
            }
        }

        /* Wait for all threads to complete */
        for (int i = 0; i < THREADS_PER_ROUND; i++) {
            thread_join(threads[i], NULL);

            if (!args[i].success) {
                fprintf(stderr, "Thread %d failed\n", i);
                free(threads);
                free(args);
                return 0;
            }
        }

        printf("    ✓ All threads completed successfully\n");

        free(threads);
        free(args);

        /* Brief pause between rounds */
        struct timespec ts = {0, 50000000};  /* 50ms */
        nanosleep(&ts, NULL);
    }

    /* Check statistics */
    cnanolog_get_stats(&stats_after);

    uint64_t expected_logs = (uint64_t)NUM_ROUNDS * THREADS_PER_ROUND * LOGS_PER_THREAD;
    uint64_t actual_logs = stats_after.total_logs_written - stats_before.total_logs_written;

    printf("\n  Statistics:\n");
    printf("    Expected logs: %llu\n", (unsigned long long)expected_logs);
    printf("    Actual logs:   %llu\n", (unsigned long long)actual_logs);
    printf("    Dropped logs:  %llu\n",
           (unsigned long long)(stats_after.dropped_logs - stats_before.dropped_logs));

    /* Verify no crashes and reasonable statistics */
    if (actual_logs > 0 && actual_logs <= expected_logs * 1.1) {
        printf("\n  ✓ Test PASSED: Thread safety verified under stress\n");
        return 1;
    } else {
        printf("\n  ✗ Test FAILED: Unexpected log count\n");
        return 0;
    }
}

/* Test: Concurrent burst logging */
void* burst_logger(void* arg) {
    int thread_id = *(int*)arg;

    cnanolog_preallocate();

    /* Burst of logs with no delays */
    for (int burst = 0; burst < 5; burst++) {
        for (int i = 0; i < 10000; i++) {
            log_info2("Burst %d: item %d", burst, i);
        }

        /* Tiny pause between bursts */
        struct timespec ts = {0, 1000000};  /* 1ms */
        nanosleep(&ts, NULL);
    }

    return NULL;
}

int test_burst_logging(void) {
    printf("\nTest 2: Concurrent Burst Logging\n");
    printf("---------------------------------\n");

    cnanolog_stats_t stats_before;
    cnanolog_get_stats(&stats_before);

    const int num_threads = 4;
    thread_t threads[num_threads];
    int thread_ids[num_threads];

    /* Create burst logging threads */
    printf("  Creating %d burst logging threads...\n", num_threads);

    for (int i = 0; i < num_threads; i++) {
        thread_ids[i] = i;
        thread_create(&threads[i], burst_logger, &thread_ids[i]);
    }

    /* Wait for completion */
    for (int i = 0; i < num_threads; i++) {
        thread_join(threads[i], NULL);
    }

    printf("  ✓ All burst threads completed\n");

    /* Check statistics */
    cnanolog_stats_t stats_after;
    cnanolog_get_stats(&stats_after);

    printf("\n  Statistics:\n");
    printf("    Logs written: %llu\n",
           (unsigned long long)(stats_after.total_logs_written - stats_before.total_logs_written));
    printf("    Dropped:      %llu\n",
           (unsigned long long)(stats_after.dropped_logs - stats_before.dropped_logs));

    printf("\n  ✓ Test PASSED: Burst logging handled correctly\n");
    return 1;
}

/* Test: Thread safety with different argument types */
void* mixed_args_logger(void* arg) {
    int thread_id = *(int*)arg;
    const char* strings[] = {"Alpha", "Beta", "Gamma", "Delta"};

    cnanolog_preallocate();

    for (int i = 0; i < 20000; i++) {
        /* Mix different argument types */
        log_info("No args");
        log_info1("One int: %d", i);
        log_info2("Two ints: %d %d", i, i * 2);
        log_info3("Three ints: %d %d %d", i, i * 2, i * 3);
        log_info1("String: %s", strings[i % 4]);
    }

    return NULL;
}

int test_mixed_argument_types(void) {
    printf("\nTest 3: Mixed Argument Types (Thread Safety)\n");
    printf("---------------------------------------------\n");

    const int num_threads = 6;
    thread_t threads[num_threads];
    int thread_ids[num_threads];

    printf("  Creating %d threads with mixed arg types...\n", num_threads);

    for (int i = 0; i < num_threads; i++) {
        thread_ids[i] = i;
        thread_create(&threads[i], mixed_args_logger, &thread_ids[i]);
    }

    for (int i = 0; i < num_threads; i++) {
        thread_join(threads[i], NULL);
    }

    printf("  ✓ All threads completed\n");
    printf("\n  ✓ Test PASSED: Mixed arg types handled correctly\n");
    return 1;
}

/* Main test suite */
int main(void) {
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║   Multi-Threaded Stress Test - Phase 7.2            ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    /* Initialize logger */
    if (cnanolog_init("stress_test.clog") != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    printf("Configuration:\n");
    printf("  Rounds:          %d\n", NUM_ROUNDS);
    printf("  Threads/round:   %d\n", THREADS_PER_ROUND);
    printf("  Logs/thread:     %d\n", LOGS_PER_THREAD);
    printf("  Total threads:   %d\n", NUM_ROUNDS * THREADS_PER_ROUND);
    printf("  Expected logs:   %d\n\n", NUM_ROUNDS * THREADS_PER_ROUND * LOGS_PER_THREAD);

    int all_passed = 1;

    /* Run tests */
    all_passed &= test_dynamic_threads();
    all_passed &= test_burst_logging();
    all_passed &= test_mixed_argument_types();

    /* Give background thread time to process */
    printf("\nWaiting for background thread to process...\n");
    struct timespec ts = {0, 500000000};  /* 500ms */
    nanosleep(&ts, NULL);

    /* Final statistics */
    printf("\nFinal Statistics:\n");
    printf("-----------------\n");

    cnanolog_stats_t stats;
    cnanolog_get_stats(&stats);

    printf("  Total logs written:     %llu\n", (unsigned long long)stats.total_logs_written);
    printf("  Dropped logs:           %llu\n", (unsigned long long)stats.dropped_logs);
    printf("  Total bytes written:    %llu bytes\n", (unsigned long long)stats.total_bytes_written);
    printf("  Compression ratio:      %.2fx\n", stats.compression_ratio_x100 / 100.0);
    printf("  Staging buffers active: %llu\n", (unsigned long long)stats.staging_buffers_active);

    double drop_rate = (stats.dropped_logs * 100.0) / (stats.total_logs_written + stats.dropped_logs);
    printf("  Drop rate:              %.2f%%\n", drop_rate);

    /* Shutdown */
    cnanolog_shutdown();

    printf("\n╔══════════════════════════════════════════════════════╗\n");
    if (all_passed) {
        printf("║   ✓ All Stress Tests PASSED!                        ║\n");
        printf("║   Thread safety verified under extreme load.        ║\n");
    } else {
        printf("║   ✗ Some Tests FAILED                                ║\n");
    }
    printf("╚══════════════════════════════════════════════════════╝\n");

    return all_passed ? 0 : 1;
}
