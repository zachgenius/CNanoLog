# Usage Guide

Detailed usage examples and patterns for CNanoLog.

## Table of Contents

- [Basic Logging](#basic-logging)
- [Output Formats](#output-formats)
- [Log Rotation](#log-rotation)
- [Multi-threaded Applications](#multi-threaded-applications)
- [Production Monitoring](#production-monitoring)
- [Custom Log Levels](#custom-log-levels)
- [Decompressing Binary Logs](#decompressing-binary-logs)

## Basic Logging

### Simple initialization

```c
#include <cnanolog.h>

int main(void) {
    // Initialize with binary output (default)
    if (cnanolog_init("app.clog") != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    // Log messages
    LOG_INFO("Application started");
    LOG_WARN("Low memory: %d MB available", available_mb);
    LOG_ERROR("Connection failed: %s (code: %d)", error_msg, error_code);
    LOG_DEBUG("Debug info: x=%d y=%d", x, y);

    // Shutdown and flush
    cnanolog_shutdown();
    return 0;
}
```

### Supported log levels

```c
LOG_INFO(format, ...)   // Informational messages
LOG_WARN(format, ...)   // Warnings
LOG_ERROR(format, ...)  // Errors
LOG_DEBUG(format, ...)  // Debug information
```

All macros support 0-50 arguments automatically.

## Output Formats

### Binary mode (default)

Maximum performance with compression. Requires decompressor tool.

```c
cnanolog_init("app.clog");

LOG_INFO("Server started on port %d", 8080);

// Decompress later:
// ./decompressor app.clog > app.log
```

### Text mode

Human-readable output with customizable format patterns.

```c
cnanolog_rotation_config_t config = {
    .policy = CNANOLOG_ROTATE_NONE,
    .base_path = "app.log",
    .format = CNANOLOG_OUTPUT_TEXT,
    .text_pattern = "%T [%l] %m"  // HH:MM:SS [LEVEL] message
};

cnanolog_init_ex(&config);

LOG_INFO("Server started on port %d", 8080);
// Output: 11:23:45.123 [INFO] Server started on port 8080
```

### Text format patterns

Available tokens:

- `%t` - Full timestamp (YYYY-MM-DD HH:MM:SS.nnnnnnnnn)
- `%T` - Short timestamp (HH:MM:SS.nnn)
- `%d` - Date only (YYYY-MM-DD)
- `%D` - Time only (HH:MM:SS)
- `%l` - Log level name (INFO, WARN, ERROR, DEBUG)
- `%L` - Log level letter (I, W, E, D)
- `%f` - Filename (basename)
- `%F` - Full file path
- `%n` - Line number
- `%m` - Formatted message
- `%%` - Literal %

**Common patterns:**

```c
// Default (if text_pattern is NULL)
.text_pattern = "[%t] [%l] [%f:%n] %m"

// Compact
.text_pattern = "%T %L %m"

// JSON
.text_pattern = "{\"time\":\"%t\",\"level\":\"%l\",\"file\":\"%f\",\"line\":%n,\"msg\":\"%m\"}"

// Logfmt
.text_pattern = "time=\"%t\" level=%l file=%f:%n msg=\"%m\""

// Syslog style
.text_pattern = "%d %T myapp[%l]: %m"

// Minimal (timestamp + message)
.text_pattern = "%T %m"

// Level first
.text_pattern = "[%l] %T - %m"
```

## Log Rotation

### Daily rotation

Automatically rotate logs at midnight (local time).

```c
cnanolog_rotation_config_t config = {
    .policy = CNANOLOG_ROTATE_DAILY,
    .base_path = "logs/app.log",
    .format = CNANOLOG_OUTPUT_TEXT
};

cnanolog_init_ex(&config);

// Files created:
// logs/app-2025-11-08.log
// logs/app-2025-11-09.log (after midnight)
// logs/app-2025-11-10.log (after next midnight)
```

### No rotation

Single log file (default behavior).

```c
cnanolog_rotation_config_t config = {
    .policy = CNANOLOG_ROTATE_NONE,
    .base_path = "app.log",
    .format = CNANOLOG_OUTPUT_TEXT
};

cnanolog_init_ex(&config);
```

## Multi-threaded Applications

### Thread-local buffers

Each thread gets its own staging buffer automatically. For best performance, preallocate at thread start:

```c
void* worker_thread(void* arg) {
    // Preallocate staging buffer to avoid first-log allocation overhead
    cnanolog_preallocate();

    int thread_id = *(int*)arg;
    LOG_INFO("Worker %d started", thread_id);

    // Logging is lock-free - no contention between threads
    for (int i = 0; i < 1000; i++) {
        LOG_INFO("Worker %d: processing item %d", thread_id, i);
    }

    LOG_INFO("Worker %d finished", thread_id);
    return NULL;
}

int main(void) {
    cnanolog_init("app.clog");

    pthread_t threads[4];
    int thread_ids[4];

    for (int i = 0; i < 4; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, worker_thread, &thread_ids[i]);
    }

    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }

    cnanolog_shutdown();
    return 0;
}
```

### CPU affinity for high performance

Pin background writer thread to dedicated core for 3x+ throughput improvement:

```c
#include <unistd.h>  // For sysconf()

int main(void) {
    cnanolog_init("app.clog");

    // Pin to last CPU core
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cores >= 4) {
        cnanolog_set_writer_affinity(num_cores - 1);
    }

    // Rest of application...
}
```

Benefits:
- Eliminates thread migration overhead
- Improves cache locality
- More predictable latency
- Higher throughput under load

## Production Monitoring

### Statistics API

Monitor logging health at runtime:

```c
void check_logging_health(void) {
    cnanolog_stats_t stats;
    cnanolog_get_stats(&stats);

    printf("Logs written: %llu\n", stats.total_logs_written);
    printf("Logs dropped: %llu\n", stats.dropped_logs);
    printf("Bytes written: %llu\n", stats.total_bytes_written);
    printf("Compression: %.2fx\n", stats.compression_ratio_x100 / 100.0);
    printf("Active buffers: %llu\n", stats.staging_buffers_active);

    // Alert if drop rate exceeds threshold
    double drop_rate = (stats.dropped_logs * 100.0) /
                       (stats.total_logs_written + stats.dropped_logs);
    if (drop_rate > 1.0) {
        fprintf(stderr, "WARNING: High drop rate: %.2f%%\n", drop_rate);
        fprintf(stderr, "Consider: increasing buffer size or setting CPU affinity\n");
    }
}
```

### Reset statistics

```c
// Reset counters without affecting operation
cnanolog_reset_stats();
```

## Custom Log Levels

Register custom log levels beyond INFO/WARN/ERROR/DEBUG:

```c
// Register custom levels (must be done before cnanolog_init)
cnanolog_register_level("METRIC", 10);
cnanolog_register_level("AUDIT", 20);
cnanolog_register_level("TRACE", 5);

// Initialize logger
cnanolog_init("app.clog");

// Use custom levels
CNANOLOG_LOG(10, "CPU usage: %d%%", cpu_usage);      // METRIC
CNANOLOG_LOG(20, "User login: %s", username);        // AUDIT
CNANOLOG_LOG(5, "Function entry: %s", __func__);     // TRACE

// Define convenience macros
#define LOG_METRIC(fmt, ...) CNANOLOG_LOG(10, fmt, ##__VA_ARGS__)
#define LOG_AUDIT(fmt, ...)  CNANOLOG_LOG(20, fmt, ##__VA_ARGS__)
#define LOG_TRACE(fmt, ...)  CNANOLOG_LOG(5, fmt, ##__VA_ARGS__)

LOG_METRIC("Request latency: %d ms", latency);
LOG_AUDIT("Permission changed: %s", resource);
```

## Decompressing Binary Logs

### Basic decompression

```bash
# Default format
./decompressor app.clog > app.log

# Output to file directly
./decompressor app.clog app.log
```

### Custom output format

```bash
# Custom format
./decompressor -f "%t | %l | %m" app.clog

# CSV format
./decompressor -f "%t,%l,%f,%n,%m" app.clog > app.csv

# Pipe to grep
./decompressor app.clog | grep ERROR

# Pipe to jq (if using JSON format)
./decompressor -f '{"time":"%t","level":"%l","msg":"%m"}' app.clog | jq .
```

### Show help

```bash
./decompressor --help
```

## Best Practices

1. **Always preallocate** in multi-threaded applications:
   ```c
   cnanolog_preallocate();  // First thing in thread function
   ```

2. **Monitor drop rate** in production:
   ```c
   if (stats.dropped_logs > 0) {
       // Investigate and tune
   }
   ```

3. **Use CPU affinity** for high-throughput applications:
   ```c
   cnanolog_set_writer_affinity(num_cores - 1);
   ```

4. **Choose appropriate output format**:
   - Binary: Maximum performance, post-processing with decompressor
   - Text: Human-readable, immediate readability, JSON/logfmt for log aggregation

5. **Use log rotation** for long-running services:
   ```c
   .policy = CNANOLOG_ROTATE_DAILY
   ```
