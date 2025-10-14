/*
 * Benchmark Adapter Interface
 *
 * Common interface that all logging libraries must implement
 * to ensure fair, apples-to-apples comparison.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Statistics Structure
 * ============================================================================ */

typedef struct {
    /* Latency (nanoseconds) */
    double latency_min_ns;
    double latency_p50_ns;
    double latency_p95_ns;
    double latency_p99_ns;
    double latency_p999_ns;
    double latency_max_ns;
    double latency_mean_ns;
    double latency_stddev_ns;

    /* Throughput */
    uint64_t total_logs_attempted;
    uint64_t total_logs_written;
    uint64_t total_drops;
    double logs_per_second;
    double mb_per_second;

    /* Resource usage */
    double cpu_percent;          /* Total CPU used by logging */
    uint64_t memory_rss_kb;      /* Resident set size */
    uint64_t memory_buffers_kb;  /* Buffer memory allocated */
    uint64_t disk_writes_kb;     /* Bytes written to disk */

    /* Reliability */
    double drop_rate_percent;
    uint64_t errors;             /* Any errors encountered */
} bench_stats_t;

/* ============================================================================
 * Benchmark Configuration
 * ============================================================================ */

typedef struct {
    /* General */
    int use_timestamps;          /* Include timestamps in logs */
    int use_async;               /* Async/sync mode */
    size_t buffer_size_bytes;    /* Per-thread buffer size */

    /* Threading */
    int num_threads;             /* Number of logging threads */
    int writer_cpu_affinity;     /* CPU core for background writer (-1 = none) */

    /* Flush policy */
    int flush_batch_size;        /* Flush every N entries */
    int flush_interval_ms;       /* OR flush every N milliseconds */
} bench_config_t;

/* ============================================================================
 * Benchmark Adapter Interface
 * ============================================================================ */

typedef struct benchmark_adapter {
    /* Library identification */
    const char* name;
    const char* version;
    const char* description;

    /* Lifecycle */
    int (*init)(const char* log_file, const bench_config_t* config);
    void (*shutdown)(void);

    /* Thread management */
    void (*thread_init)(void);   /* Call in each logging thread */
    void (*thread_cleanup)(void);

    /* Logging functions - various argument counts */
    void (*log_0_args)(const char* msg);
    void (*log_1_int)(const char* fmt, int arg);
    void (*log_2_ints)(const char* fmt, int a1, int a2);
    void (*log_4_ints)(const char* fmt, int a1, int a2, int a3, int a4);
    void (*log_8_ints)(const char* fmt, int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8);

    /* Various data types */
    void (*log_1_long)(const char* fmt, long arg);
    void (*log_1_uint64)(const char* fmt, uint64_t arg);
    void (*log_1_float)(const char* fmt, float arg);
    void (*log_1_double)(const char* fmt, double arg);
    void (*log_1_string)(const char* fmt, const char* str);

    /* Mixed types */
    void (*log_mixed)(const char* fmt, int i1, const char* s1, int i2);
    void (*log_mixed2)(const char* fmt, int i1, double d1, const char* s1);

    /* Statistics */
    void (*get_stats)(bench_stats_t* stats);
    void (*reset_stats)(void);

    /* Flush control */
    void (*flush)(void);         /* Force flush to disk */

    /* Configuration (can be called after init) */
    int (*set_cpu_affinity)(int core);
    int (*set_buffer_size)(size_t bytes);
    int (*set_async_mode)(int enabled);
} benchmark_adapter_t;

/* ============================================================================
 * Adapter Registration
 * ============================================================================ */

/* Each library provides this function */
typedef benchmark_adapter_t* (*adapter_factory_fn)(void);

/* Register adapters */
#define REGISTER_ADAPTER(name, factory) \
    static benchmark_adapter_t* __attribute__((constructor)) \
    register_##name(void) { \
        return factory(); \
    }

#ifdef __cplusplus
}
#endif
