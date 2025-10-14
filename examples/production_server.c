/*
 * Production Server Example
 *
 * Demonstrates:
 * - Complete production-ready logging setup
 * - CPU affinity configuration
 * - Log rotation
 * - Error handling
 * - Graceful shutdown
 * - Real-world server simulation
 */

#include <cnanolog.h>
#include "../src/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

/* Server configuration */
#define SERVER_PORT 8080
#define MAX_CONNECTIONS 100
#define LOG_ROTATION_SIZE_MB 100

/* Global flag for graceful shutdown */
static volatile int g_server_running = 1;

/* Signal handler for graceful shutdown */
void signal_handler(int signum) {
    (void)signum;
    printf("\n[SERVER] Shutdown signal received, stopping gracefully...\n");
    g_server_running = 0;
}

/* Setup logging with optimal configuration */
int setup_logging(void) {
    printf("[SETUP] Initializing logging system...\n");

    /* Initialize logger */
    if (cnanolog_init("server.clog") != 0) {
        fprintf(stderr, "[ERROR] Failed to initialize logger\n");
        return -1;
    }

    printf("[SETUP] Logger initialized\n");

    /* Preallocate for main thread */
    cnanolog_preallocate();
    printf("[SETUP] Main thread buffer preallocated\n");

    /* Configure CPU affinity for optimal performance */
#ifdef PLATFORM_LINUX
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(PLATFORM_MACOS)
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(PLATFORM_WINDOWS)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    int num_cores = sysinfo.dwNumberOfProcessors;
#else
    int num_cores = 4;
#endif

    printf("[SETUP] Detected %d CPU cores\n", num_cores);

    if (num_cores >= 4) {
        int target_core = num_cores - 1;
        if (cnanolog_set_writer_affinity(target_core) == 0) {
            printf("[SETUP] Writer thread pinned to core %d\n", target_core);
            log_info1("Writer thread affinity set to core %d", target_core);
        } else {
            printf("[SETUP] Failed to set CPU affinity (continuing anyway)\n");
            log_warn("Failed to set CPU affinity");
        }
    } else {
        printf("[SETUP] Skipping CPU affinity (only %d cores)\n", num_cores);
    }

    log_info("Logging system initialized successfully");
    return 0;
}

/* Check and rotate log if needed */
void check_log_rotation(void) {
    cnanolog_stats_t stats;
    cnanolog_get_stats(&stats);

    uint64_t size_mb = stats.total_bytes_written / (1024 * 1024);

    if (size_mb >= LOG_ROTATION_SIZE_MB) {
        log_warn1("Log rotation triggered (size: %llu MB)", (unsigned long long)size_mb);

        printf("[ROTATE] Rotating log file (size: %llu MB)\n", (unsigned long long)size_mb);

        /* Shutdown current logger */
        cnanolog_shutdown();

        /* Rename old log */
        time_t now = time(NULL);
        char old_name[256];
        snprintf(old_name, sizeof(old_name), "server_%ld.clog", (long)now);

        if (rename("server.clog", old_name) == 0) {
            printf("[ROTATE] Renamed to: %s\n", old_name);
        }

        /* Restart logger with new file */
        if (cnanolog_init("server.clog") != 0) {
            fprintf(stderr, "[ERROR] Failed to reinitialize logger after rotation\n");
            exit(1);
        }

        cnanolog_preallocate();
        log_info1("Log rotated, old file: %s", old_name);

        printf("[ROTATE] Log rotation complete\n");
    }
}

/* Simulate client request handler */
void handle_client_request(int client_id, const char* request_type) {
    /* Simulate different types of requests */
    int processing_time_ms = rand() % 100;  // 0-100ms

    log_info3("Client %d: %s request received (expected: %d ms)",
             client_id, request_type, processing_time_ms);

    /* Simulate processing */
    usleep(processing_time_ms * 1000);

    /* Simulate occasional errors */
    int success = rand() % 100;

    if (success < 95) {
        /* Success */
        int status_code = 200;
        log_info3("Client %d: %s completed (status: %d)",
                 client_id, request_type, status_code);
    } else if (success < 98) {
        /* Client error */
        int status_code = 400;
        log_warn3("Client %d: %s failed (status: %d)",
                 client_id, request_type, status_code);
    } else {
        /* Server error */
        int status_code = 500;
        const char* error = "Internal server error";
        log_error3("Client %d: %s failed - %s (status: %d)",
                  client_id, request_type, error);
    }
}

/* Worker thread to handle requests */
void* worker_thread(void* arg) {
    int worker_id = *(int*)arg;

    cnanolog_preallocate();

    log_info1("Worker thread %d started", worker_id);

    const char* request_types[] = {
        "GET", "POST", "PUT", "DELETE"
    };

    while (g_server_running) {
        /* Simulate receiving requests */
        int client_id = rand() % MAX_CONNECTIONS;
        const char* request_type = request_types[rand() % 4];

        handle_client_request(client_id, request_type);

        /* Small delay between requests */
        usleep(10000 + rand() % 20000);  // 10-30ms
    }

    log_info1("Worker thread %d stopped", worker_id);

    return NULL;
}

/* Monitoring thread */
void* monitoring_thread(void* arg) {
    (void)arg;

    cnanolog_preallocate();

    log_info("Monitoring thread started");

    cnanolog_stats_t prev_stats = {0};

    while (g_server_running) {
        sleep(5);

        cnanolog_stats_t stats;
        cnanolog_get_stats(&stats);

        /* Calculate deltas */
        uint64_t logs_delta = stats.total_logs_written - prev_stats.total_logs_written;
        uint64_t drops_delta = stats.dropped_logs - prev_stats.dropped_logs;

        double drop_rate = 0.0;
        if (logs_delta > 0) {
            drop_rate = (drops_delta * 100.0) / (logs_delta + drops_delta);
        }

        /* Log monitoring report */
        log_info2("Monitor: %llu logs, drop_rate=%.2f%%",
                 (unsigned long long)logs_delta, (int)(drop_rate * 100));

        if (drop_rate > 1.0) {
            log_warn1("High drop rate detected: %.2f%%", (int)(drop_rate * 100));
        }

        prev_stats = stats;

        /* Check for log rotation */
        check_log_rotation();
    }

    log_info("Monitoring thread stopped");

    return NULL;
}

int main(void) {
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║   Production Server Example                          ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    /* Seed random number generator */
    srand(time(NULL));

    /* Setup signal handlers for graceful shutdown */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("[SERVER] Setting up signal handlers\n");

    /* Setup logging */
    if (setup_logging() != 0) {
        return 1;
    }

    log_info("Server starting");
    log_info1("Server port: %d", SERVER_PORT);

    printf("\n[SERVER] Server started on port %d\n", SERVER_PORT);
    printf("[SERVER] Press Ctrl+C to stop gracefully\n\n");

    /* Start monitoring thread */
    cnanolog_thread_t monitor_tid;
    if (cnanolog_thread_create(&monitor_tid, monitoring_thread, NULL) != 0) {
        log_error("Failed to start monitoring thread");
        return 1;
    }

    /* Start worker threads */
    const int num_workers = 4;
    cnanolog_thread_t worker_tids[num_workers];
    int worker_ids[num_workers];

    for (int i = 0; i < num_workers; i++) {
        worker_ids[i] = i;
        if (cnanolog_thread_create(&worker_tids[i], worker_thread, &worker_ids[i]) != 0) {
            log_error1("Failed to start worker thread %d", i);
            return 1;
        }
    }

    log_info1("Started %d worker threads", num_workers);
    printf("[SERVER] Started %d worker threads\n", num_workers);
    printf("[SERVER] Server is running...\n\n");

    /* Main server loop */
    while (g_server_running) {
        sleep(1);
    }

    /* Graceful shutdown */
    printf("\n[SHUTDOWN] Stopping server...\n");
    log_info("Server shutdown initiated");

    /* Wait for all threads to complete */
    printf("[SHUTDOWN] Waiting for worker threads...\n");
    for (int i = 0; i < num_workers; i++) {
        cnanolog_thread_join(worker_tids[i], NULL);
    }

    printf("[SHUTDOWN] Waiting for monitoring thread...\n");
    cnanolog_thread_join(monitor_tid, NULL);

    log_info("All threads stopped");

    /* Final statistics */
    printf("\n╔══════════════════════════════════════════════════════╗\n");
    printf("║   Server Statistics                                  ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");

    cnanolog_stats_t final_stats;
    cnanolog_get_stats(&final_stats);

    printf("  Total logs written:     %llu\n", (unsigned long long)final_stats.total_logs_written);
    printf("  Total bytes written:    %llu bytes (%.2f MB)\n",
           (unsigned long long)final_stats.total_bytes_written,
           final_stats.total_bytes_written / (1024.0 * 1024.0));
    printf("  Dropped logs:           %llu\n", (unsigned long long)final_stats.dropped_logs);
    printf("  Compression ratio:      %.2fx\n", final_stats.compression_ratio_x100 / 100.0);
    printf("  Background wakeups:     %llu\n", (unsigned long long)final_stats.background_wakeups);

    double overall_drop_rate = 0.0;
    if (final_stats.total_logs_written > 0) {
        overall_drop_rate = (final_stats.dropped_logs * 100.0) /
                           (final_stats.total_logs_written + final_stats.dropped_logs);
    }
    printf("  Drop rate:              %.2f%%\n", overall_drop_rate);

    /* Shutdown logger */
    printf("\n[SHUTDOWN] Shutting down logger...\n");
    cnanolog_shutdown();

    log_info("Server shutdown complete");

    printf("\n╔══════════════════════════════════════════════════════╗\n");
    printf("║   Server Stopped Successfully                        ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");

    printf("\nTo view server logs, run:\n");
    printf("  ../tools/decompressor server.clog | less\n");
    printf("\nTo view only errors:\n");
    printf("  ../tools/decompressor server.clog | grep ERROR\n\n");

    return 0;
}
