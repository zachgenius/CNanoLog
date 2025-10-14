/*
 * CNanoLog Benchmark Adapter
 *
 * Implements the benchmark_adapter_t interface for CNanoLog.
 */

#include "../common/benchmark_adapter.h"
#include "../../include/cnanolog.h"
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

/* ============================================================================
 * Global State
 * ============================================================================ */

static bench_config_t g_config;
static char g_log_file[256];

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t get_memory_usage_kb(void) {
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        #ifdef __APPLE__
            return usage.ru_maxrss / 1024;  /* macOS returns bytes */
        #else
            return usage.ru_maxrss;  /* Linux returns KB */
        #endif
    }
    return 0;
}

static uint64_t get_file_size(const char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) return 0;

    fseek(fp, 0, SEEK_END);
    uint64_t size = ftell(fp);
    fclose(fp);

    return size;
}

/* ============================================================================
 * Adapter Implementation
 * ============================================================================ */

static int cnanolog_adapter_init(const char* log_file, const bench_config_t* config) {
    if (!log_file || !config) return -1;

    /* Save configuration */
    memcpy(&g_config, config, sizeof(bench_config_t));
    strncpy(g_log_file, log_file, sizeof(g_log_file) - 1);

    /* Initialize CNanoLog */
    if (cnanolog_init(log_file) != 0) {
        fprintf(stderr, "CNanoLog: Failed to initialize\n");
        return -1;
    }

    /* Set CPU affinity if requested */
    if (config->writer_cpu_affinity >= 0) {
        if (cnanolog_set_writer_affinity(config->writer_cpu_affinity) != 0) {
            fprintf(stderr, "CNanoLog: Warning - failed to set CPU affinity\n");
        }
    }

    return 0;
}

static void cnanolog_adapter_shutdown(void) {
    cnanolog_shutdown();
}

static void cnanolog_adapter_thread_init(void) {
    cnanolog_preallocate();
}

static void cnanolog_adapter_thread_cleanup(void) {
    /* Nothing to do - CNanoLog handles cleanup automatically */
}

/* Logging functions */
static void cnanolog_adapter_log_0_args(const char* msg) {
    log_info(msg);
}

static void cnanolog_adapter_log_1_int(const char* fmt, int arg) {
    log_info1(fmt, arg);
}

static void cnanolog_adapter_log_2_ints(const char* fmt, int a1, int a2) {
    log_info2(fmt, a1, a2);
}

static void cnanolog_adapter_log_4_ints(const char* fmt, int a1, int a2,
                                         int a3, int a4) {
    log_info4(fmt, a1, a2, a3, a4);
}

static void cnanolog_adapter_log_8_ints(const char* fmt, int a1, int a2,
                                         int a3, int a4, int a5, int a6,
                                         int a7, int a8) {
    log_info8(fmt, a1, a2, a3, a4, a5, a6, a7, a8);
}

static void cnanolog_adapter_log_1_long(const char* fmt, long arg) {
    log_info1(fmt, arg);
}

static void cnanolog_adapter_log_1_uint64(const char* fmt, uint64_t arg) {
    log_info1(fmt, arg);
}

static void cnanolog_adapter_log_1_float(const char* fmt, float arg) {
    log_info1(fmt, arg);
}

static void cnanolog_adapter_log_1_double(const char* fmt, double arg) {
    log_info1(fmt, arg);
}

static void cnanolog_adapter_log_1_string(const char* fmt, const char* str) {
    log_info1(fmt, str);
}

static void cnanolog_adapter_log_mixed(const char* fmt, int i1,
                                        const char* s1, int i2) {
    log_info3(fmt, i1, s1, i2);
}

static void cnanolog_adapter_log_mixed2(const char* fmt, int i1, double d1,
                                         const char* s1) {
    log_info3(fmt, i1, d1, s1);
}

static void cnanolog_adapter_get_stats(bench_stats_t* stats) {
    if (!stats) return;

    /* Get CNanoLog stats */
    cnanolog_stats_t cstats;
    cnanolog_get_stats(&cstats);

    /* Convert to benchmark stats format */
    memset(stats, 0, sizeof(bench_stats_t));

    /* Throughput stats */
    stats->total_logs_attempted = cstats.total_logs_written + cstats.dropped_logs;
    stats->total_logs_written = cstats.total_logs_written;
    stats->total_drops = cstats.dropped_logs;

    /* Calculate drop rate */
    if (stats->total_logs_attempted > 0) {
        stats->drop_rate_percent =
            (cstats.dropped_logs * 100.0) / stats->total_logs_attempted;
    }

    /* Resource usage */
    stats->memory_rss_kb = get_memory_usage_kb();
    stats->disk_writes_kb = get_file_size(g_log_file) / 1024;

    /* Note: Latency stats are measured externally by the benchmark harness */
}

static void cnanolog_adapter_reset_stats(void) {
    cnanolog_reset_stats();
}

static void cnanolog_adapter_flush(void) {
    /* CNanoLog flushes asynchronously - sleep briefly to allow flush */
    usleep(100000);  /* 100ms */
}

static int cnanolog_adapter_set_cpu_affinity(int core) {
    return cnanolog_set_writer_affinity(core);
}

static int cnanolog_adapter_set_buffer_size(size_t bytes) {
    /* Buffer size is compile-time in CNanoLog */
    fprintf(stderr, "CNanoLog: Buffer size cannot be changed at runtime\n");
    fprintf(stderr, "         (recompile with different STAGING_BUFFER_SIZE)\n");
    return -1;
}

static int cnanolog_adapter_set_async_mode(int enabled) {
    /* CNanoLog is always async */
    if (!enabled) {
        fprintf(stderr, "CNanoLog: Cannot disable async mode (always async)\n");
        return -1;
    }
    return 0;
}

/* ============================================================================
 * Adapter Registration
 * ============================================================================ */

static benchmark_adapter_t g_cnanolog_adapter = {
    .name = "CNanoLog",
    .version = "1.0.0",
    .description = "Binary format, lock-free logging library",

    .init = cnanolog_adapter_init,
    .shutdown = cnanolog_adapter_shutdown,

    .thread_init = cnanolog_adapter_thread_init,
    .thread_cleanup = cnanolog_adapter_thread_cleanup,

    .log_0_args = cnanolog_adapter_log_0_args,
    .log_1_int = cnanolog_adapter_log_1_int,
    .log_2_ints = cnanolog_adapter_log_2_ints,
    .log_4_ints = cnanolog_adapter_log_4_ints,
    .log_8_ints = cnanolog_adapter_log_8_ints,

    .log_1_long = cnanolog_adapter_log_1_long,
    .log_1_uint64 = cnanolog_adapter_log_1_uint64,
    .log_1_float = cnanolog_adapter_log_1_float,
    .log_1_double = cnanolog_adapter_log_1_double,
    .log_1_string = cnanolog_adapter_log_1_string,

    .log_mixed = cnanolog_adapter_log_mixed,
    .log_mixed2 = cnanolog_adapter_log_mixed2,

    .get_stats = cnanolog_adapter_get_stats,
    .reset_stats = cnanolog_adapter_reset_stats,

    .flush = cnanolog_adapter_flush,

    .set_cpu_affinity = cnanolog_adapter_set_cpu_affinity,
    .set_buffer_size = cnanolog_adapter_set_buffer_size,
    .set_async_mode = cnanolog_adapter_set_async_mode,
};

benchmark_adapter_t* get_cnanolog_adapter(void) {
    return &g_cnanolog_adapter;
}
