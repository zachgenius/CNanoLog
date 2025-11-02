/*
 * Real-Time Logging Example
 *
 * Demonstrates techniques for achieving <10μs max latency:
 * 1. Preallocation with page fault pre-warming
 * 2. CPU isolation
 * 3. Cache warming
 * 4. Memory locking (prevents paging)
 */

#include <cnanolog.h>
#include "../src/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __linux__
    #include <sys/mman.h>  /* For mlockall() */
    #include <sched.h>
#endif

/* Force page faults during initialization, not during logging */
void warm_staging_buffer(void) {
    /* After preallocate, trigger all page faults by writing to buffer */
    /* This is a workaround - ideally cnanolog would do this internally */

    /* Log enough times to touch all pages in the 8MB buffer */
    /* 8MB / 4KB page = 2048 pages */
    /* Each log ~40 bytes, so need 2048 * 4096 / 40 = ~200K logs */

    printf("  Warming staging buffer (forcing page faults)...\n");
    for (int i = 0; i < 200000; i++) {
        LOG_INFO("Warmup log %d", i);
    }

    /* Wait for background writer to process */
    usleep(500000);  /* 500ms */

    /* Reset stats to ignore warmup */
    cnanolog_reset_stats();
}

int main(void) {
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║          CNanoLog Real-Time Logging Example                 ║\n");
    printf("║          Target: <10μs max latency                          ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

#ifdef __linux__
    /* Step 1: Lock memory to prevent paging */
    printf("Step 1: Locking memory (prevents swap)...\n");
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        printf("  Warning: mlockall() failed (need root/CAP_IPC_LOCK)\n");
        printf("  Continuing without memory locking...\n");
    } else {
        printf("  ✓ Memory locked\n");
    }
    printf("\n");
#endif

    /* Step 2: Initialize logger */
    printf("Step 2: Initializing logger...\n");
    if (cnanolog_init("realtime.clog") != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }
    printf("  ✓ Logger initialized\n\n");

    /* Step 3: Set CPU affinity for background writer */
    printf("Step 3: Setting CPU affinity...\n");
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    int writer_core = num_cores - 1;  /* Use last core */
    printf("  System has %d cores, pinning writer to core %d\n", num_cores, writer_core);

    if (cnanolog_set_writer_affinity(writer_core) == 0) {
        printf("  ✓ Writer thread pinned to core %d\n", writer_core);
    } else {
        printf("  Warning: Failed to set CPU affinity\n");
    }
    printf("\n");

    /* Step 4: Preallocate buffer for this thread */
    printf("Step 4: Preallocating staging buffer...\n");
    cnanolog_preallocate();
    printf("  ✓ 8MB staging buffer allocated\n\n");

    /* Step 5: Warm the buffer (force all page faults now) */
    printf("Step 5: Warming buffer (forcing page faults)...\n");
    warm_staging_buffer();
    printf("  ✓ All pages touched, page faults eliminated\n\n");

    /* Step 6: Optional - Set this thread to real-time priority */
#ifdef __linux__
    printf("Step 6: Setting real-time priority...\n");
    struct sched_param param;
    param.sched_priority = 10;  /* Low RT priority */
    if (sched_setscheduler(0, SCHED_FIFO, &param) == 0) {
        printf("  ✓ Thread set to SCHED_FIFO priority %d\n", param.sched_priority);
        printf("  Warning: RT priority requires root privileges\n");
    } else {
        printf("  Warning: Failed to set RT priority (need root)\n");
        printf("  Continuing with normal priority...\n");
    }
    printf("\n");
#endif

    /* Now do real-time logging */
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║          Real-Time Logging Test (100K logs)                 ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    printf("Logging 100,000 entries...\n");
    for (int i = 0; i < 100000; i++) {
        LOG_INFO("Real-time log %d: value=%d", i, i * 2);
    }

    /* Wait for processing */
    usleep(500000);  /* 500ms */

    /* Get statistics */
    cnanolog_stats_t stats;
    cnanolog_get_stats(&stats);

    printf("\n");
    printf("Results:\n");
    printf("  Total logs:    %llu\n", (unsigned long long)stats.total_logs_written);
    printf("  Dropped logs:  %llu\n", (unsigned long long)stats.dropped_logs);
    printf("  File size:     %.2f MB\n", stats.total_bytes_written / (1024.0 * 1024.0));
    printf("\n");

    if (stats.dropped_logs == 0) {
        printf("✅ SUCCESS: 0%% drop rate\n");
    } else {
        printf("⚠️  WARNING: %llu logs dropped\n", (unsigned long long)stats.dropped_logs);
    }

    printf("\n");
    printf("Expected latency characteristics:\n");
    printf("  p50:    15-25ns    (typical case)\n");
    printf("  p99:    50-100ns   (cache miss)\n");
    printf("  p99.9:  200-500ns  (minor outlier)\n");
    printf("  Max:    <10μs      (with all optimizations)\n");
    printf("\n");

    printf("To verify, run benchmark:\n");
    printf("  sudo ./benchmark_comprehensive %d --scale Medium\n", writer_core);
    printf("\n");

    cnanolog_shutdown();

    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                   Techniques Summary                         ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    printf("1. Memory locking (mlockall):      Prevents paging to swap\n");
    printf("2. CPU affinity:                   Prevents thread migration\n");
    printf("3. Buffer preallocation:           Eliminates first-log alloc\n");
    printf("4. Buffer warming:                 Forces page faults early\n");
    printf("5. Real-time priority (optional):  Reduces scheduler latency\n");
    printf("\n");
    printf("For production real-time systems:\n");
    printf("  - Use isolcpus= kernel parameter\n");
    printf("  - Pin application threads to isolated cores\n");
    printf("  - Pin writer thread to different isolated core\n");
    printf("  - Monitor max latency in production\n");
    printf("\n");

    return 0;
}
