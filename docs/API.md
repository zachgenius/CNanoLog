# API Reference

Complete API documentation for CNanoLog.

## Table of Contents

- [Initialization](#initialization)
- [Logging Macros](#logging-macros)
- [Configuration Types](#configuration-types)
- [Statistics](#statistics)
- [Thread Management](#thread-management)
- [Custom Log Levels](#custom-log-levels)
- [Internal API](#internal-api)

## Initialization

### cnanolog_init

```c
int cnanolog_init(const char* log_file_path);
```

Initialize logging system with binary format and no rotation.

**Parameters:**
- `log_file_path` - Path to binary log file (e.g., "app.clog")

**Returns:**
- `0` on success
- `-1` on failure

**Example:**
```c
if (cnanolog_init("app.clog") != 0) {
    fprintf(stderr, "Failed to initialize logger\n");
    return 1;
}
```

### cnanolog_init_ex

```c
int cnanolog_init_ex(const cnanolog_rotation_config_t* config);
```

Initialize logging system with advanced options (rotation, output format, custom patterns).

**Parameters:**
- `config` - Configuration structure (see [Configuration Types](#configuration-types))

**Returns:**
- `0` on success
- `-1` on failure

**Example:**
```c
cnanolog_rotation_config_t config = {
    .policy = CNANOLOG_ROTATE_DAILY,
    .base_path = "logs/app.log",
    .format = CNANOLOG_OUTPUT_TEXT,
    .text_pattern = "%T [%l] %m"
};

if (cnanolog_init_ex(&config) != 0) {
    fprintf(stderr, "Failed to initialize logger\n");
    return 1;
}
```

### cnanolog_shutdown

```c
void cnanolog_shutdown(void);
```

Shutdown logging system, flush all pending logs, and release resources.

**Thread safety:** Must be called from main thread after all logging threads have finished.

**Example:**
```c
cnanolog_shutdown();
```

## Logging Macros

### LOG_INFO

```c
LOG_INFO(format, ...)
```

Log informational message.

**Parameters:**
- `format` - printf-style format string (compile-time literal)
- `...` - 0-50 arguments matching format specifiers

**Example:**
```c
LOG_INFO("Application started");
LOG_INFO("User %s logged in from %s", username, ip_address);
LOG_INFO("Position: x=%d y=%d z=%d", x, y, z);
```

### LOG_WARN

```c
LOG_WARN(format, ...)
```

Log warning message.

**Example:**
```c
LOG_WARN("Low memory: %d MB available", available_mb);
```

### LOG_ERROR

```c
LOG_ERROR(format, ...)
```

Log error message.

**Example:**
```c
LOG_ERROR("Connection failed: %s (code: %d)", error_msg, error_code);
```

### LOG_DEBUG

```c
LOG_DEBUG(format, ...)
```

Log debug message.

**Example:**
```c
LOG_DEBUG("Function %s called with arg=%d", __func__, arg);
```

### CNANOLOG_LOG

```c
CNANOLOG_LOG(level, format, ...)
```

Log message with custom log level.

**Parameters:**
- `level` - Log level value (0-3 for standard levels, 4-255 for custom)
- `format` - printf-style format string
- `...` - 0-50 arguments

**Example:**
```c
CNANOLOG_LOG(10, "CPU usage: %d%%", cpu_usage);
```

## Configuration Types

### cnanolog_rotation_policy_t

```c
typedef enum {
    CNANOLOG_ROTATE_NONE = 0,    // No rotation (default)
    CNANOLOG_ROTATE_DAILY,        // Rotate at midnight (local time)
} cnanolog_rotation_policy_t;
```

### cnanolog_output_format_t

```c
typedef enum {
    CNANOLOG_OUTPUT_BINARY = 0,  // Binary format (default) - requires decompressor
    CNANOLOG_OUTPUT_TEXT = 1,    // Human-readable text format
} cnanolog_output_format_t;
```

### cnanolog_rotation_config_t

```c
typedef struct {
    cnanolog_rotation_policy_t policy;  // Rotation policy
    const char* base_path;              // Base path (e.g., "logs/app.log")
    cnanolog_output_format_t format;    // Output format (binary or text)
    const char* text_pattern;           // Text format pattern (NULL = use default)
                                        // Only applies when format == CNANOLOG_OUTPUT_TEXT
} cnanolog_rotation_config_t;
```

**Fields:**
- `policy` - Rotation policy (NONE or DAILY)
- `base_path` - Base file path; dated files use pattern "base-YYYY-MM-DD.ext"
- `format` - Output format (binary or text)
- `text_pattern` - Custom format pattern for text mode (NULL = default pattern)

**Default text pattern:**
```c
"[%t] [%l] [%f:%n] %m"
```

**Text pattern tokens:**
- `%t` - Full timestamp (YYYY-MM-DD HH:MM:SS.nnnnnnnnn)
- `%T` - Short timestamp (HH:MM:SS.nnn)
- `%d` - Date (YYYY-MM-DD)
- `%D` - Time (HH:MM:SS)
- `%l` - Level name (INFO, WARN, ERROR, DEBUG)
- `%L` - Level letter (I, W, E, D)
- `%f` - Filename
- `%F` - Full file path
- `%n` - Line number
- `%m` - Message
- `%%` - Literal %

### cnanolog_level_t

```c
typedef enum {
    LOG_LEVEL_INFO  = 0,
    LOG_LEVEL_WARN  = 1,
    LOG_LEVEL_ERROR = 2,
    LOG_LEVEL_DEBUG = 3
} cnanolog_level_t;
```

Custom levels can use values 4-255.

## Statistics

### cnanolog_stats_t

```c
typedef struct {
    uint64_t total_logs_written;     // Total log entries written
    uint64_t total_bytes_written;    // Total bytes written to file
    uint64_t dropped_logs;           // Logs dropped due to full buffers
    uint64_t compression_ratio_x100; // e.g., 350 = 3.50x compression
    uint64_t staging_buffers_active; // Number of thread-local buffers
    uint64_t background_wakeups;     // Background thread wake count
} cnanolog_stats_t;
```

### cnanolog_get_stats

```c
void cnanolog_get_stats(cnanolog_stats_t* stats);
```

Get current logging statistics.

**Parameters:**
- `stats` - Pointer to structure to fill with statistics

**Thread safety:** Can be called at any time from any thread.

**Example:**
```c
cnanolog_stats_t stats;
cnanolog_get_stats(&stats);

printf("Logs written: %llu\n", stats.total_logs_written);
printf("Logs dropped: %llu\n", stats.dropped_logs);
printf("Compression: %.2fx\n", stats.compression_ratio_x100 / 100.0);
```

### cnanolog_reset_stats

```c
void cnanolog_reset_stats(void);
```

Reset statistics counters to zero. Does not affect operational state.

**Example:**
```c
cnanolog_reset_stats();
```

## Thread Management

### cnanolog_preallocate

```c
void cnanolog_preallocate(void);
```

Preallocate thread-local buffer for the calling thread.

**When to use:**
- Call at the start of each logging thread
- Avoids first-log allocation overhead
- Provides more predictable latency

**Example:**
```c
void* worker_thread(void* arg) {
    cnanolog_preallocate();  // First thing in thread
    LOG_INFO("Worker started");
    // ... rest of thread ...
}
```

### cnanolog_set_writer_affinity

```c
int cnanolog_set_writer_affinity(int core_id);
```

Pin background writer thread to specific CPU core.

**Parameters:**
- `core_id` - CPU core ID (0-based, typically 0 to num_cores-1)

**Returns:**
- `0` on success
- `-1` on failure (non-fatal - logging continues)

**Benefits:**
- 3x+ throughput improvement
- Eliminates thread migration overhead
- Improved cache locality
- More predictable latency

**Platform support:**
- Linux: Full support (pthread_setaffinity_np)
- macOS: Best-effort (thread_policy_set)
- Windows: Full support (SetThreadAffinityMask)

**Example:**
```c
cnanolog_init("app.clog");

int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
if (num_cores >= 4) {
    // Pin to last core
    cnanolog_set_writer_affinity(num_cores - 1);
}
```

## Custom Log Levels

### cnanolog_register_level

```c
int cnanolog_register_level(const char* name, uint8_t level);
```

Register a custom log level.

**Parameters:**
- `name` - Level name (e.g., "METRIC", "AUDIT", "TRACE")
- `level` - Level value (4-255; 0-3 reserved for INFO/WARN/ERROR/DEBUG)

**Returns:**
- `0` on success
- `-1` on failure

**Important:** Must be called before `cnanolog_init()` or `cnanolog_init_ex()`.

**Example:**
```c
cnanolog_register_level("METRIC", 10);
cnanolog_register_level("AUDIT", 20);

cnanolog_init("app.clog");

CNANOLOG_LOG(10, "CPU: %d%%", usage);
CNANOLOG_LOG(20, "User login: %s", username);

// Define convenience macros
#define LOG_METRIC(fmt, ...) CNANOLOG_LOG(10, fmt, ##__VA_ARGS__)
#define LOG_AUDIT(fmt, ...)  CNANOLOG_LOG(20, fmt, ##__VA_ARGS__)
```

## Internal API

The following functions are used internally by logging macros. Do not call directly.

### _cnanolog_register_site

```c
uint32_t _cnanolog_register_site(cnanolog_level_t level,
                                  const char* filename,
                                  uint32_t line_number,
                                  const char* format,
                                  uint8_t num_args,
                                  const uint8_t* arg_types);
```

Register a log site and return unique ID. Called automatically by macros on first use.

### _cnanolog_log_binary

```c
void _cnanolog_log_binary(uint32_t log_id,
                          uint8_t num_args,
                          const uint8_t* arg_types,
                          ...);
```

Write a binary log entry. Called by macros after registration.

## Thread Safety

- **Initialization:** `cnanolog_init()` and `cnanolog_init_ex()` must be called from main thread
- **Logging:** All logging macros are thread-safe and lock-free
- **Statistics:** `cnanolog_get_stats()` and `cnanolog_reset_stats()` are thread-safe
- **Shutdown:** `cnanolog_shutdown()` must be called after all logging threads finish
- **Preallocate:** `cnanolog_preallocate()` must be called from each logging thread

## Error Handling

- Initialization functions return `-1` on failure
- Logging macros never fail (drop on buffer full)
- Statistics functions never fail
- CPU affinity failure is non-fatal (logging continues)
