# Configuration and Optimization

Tuning and optimization options for CNanoLog.

## Table of Contents

- [Compile-Time Configuration](#compile-time-configuration)
- [Runtime Configuration](#runtime-configuration)
- [Performance Optimization](#performance-optimization)
- [Buffer Tuning](#buffer-tuning)
- [Platform-Specific Notes](#platform-specific-notes)

## Compile-Time Configuration

### Timestamp Control

Enable or disable high-resolution timestamps.

**Build with timestamps (default):**
```bash
cmake -DCNANOLOG_ENABLE_TIMESTAMPS=ON ..
make
```

**Build without timestamps (extreme performance mode):**
```bash
cmake -DCNANOLOG_ENABLE_TIMESTAMPS=OFF ..
make
```

**Benefits of disabling timestamps:**
- ~43% smaller log entries (14 bytes → 6 bytes per header)
- No rdtsc() overhead (~5-10ns saved per log)
- Faster startup (skips 100ms timestamp calibration)
- More logs fit in buffers

**When to disable timestamps:**
- High-frequency event counting
- Aggregation scenarios where timing isn't critical
- Memory-constrained embedded systems
- Maximum throughput benchmarking

The decompressor automatically handles both timestamp and no-timestamp files.

### Build Options

```bash
# Enable/disable timestamps (default: ON)
cmake -DCNANOLOG_ENABLE_TIMESTAMPS=ON ..

# Build examples (default: ON)
cmake -DBUILD_EXAMPLES=OFF ..

# Build tests (default: ON)
cmake -DBUILD_TESTS=OFF ..

# Set install prefix
cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..

# Debug build
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Release build with optimizations (default)
cmake -DCMAKE_BUILD_TYPE=Release ..
```

## Runtime Configuration

### Output Format Selection

Choose between binary (compressed) or text (human-readable) output.

**Binary mode (default):**
```c
cnanolog_init("app.clog");
```

Benefits:
- Maximum compression (~3.5x)
- Smallest log files
- Fastest throughput (~50M logs/sec)

Trade-offs:
- Requires decompressor tool
- Post-processing step needed

**Text mode:**
```c
cnanolog_rotation_config_t config = {
    .policy = CNANOLOG_ROTATE_NONE,
    .base_path = "app.log",
    .format = CNANOLOG_OUTPUT_TEXT,
    .text_pattern = NULL  // Use default pattern
};
cnanolog_init_ex(&config);
```

Benefits:
- Immediately readable
- tail -f compatible
- Structured formats (JSON, logfmt)

Trade-offs:
- Larger files (no compression)
- Lower throughput (~5-8M logs/sec)

**Note:** Producer latency is identical in both modes (~54ns).

### Log Rotation

**No rotation (default):**
```c
cnanolog_rotation_config_t config = {
    .policy = CNANOLOG_ROTATE_NONE,
    .base_path = "app.log"
};
```

**Daily rotation:**
```c
cnanolog_rotation_config_t config = {
    .policy = CNANOLOG_ROTATE_DAILY,
    .base_path = "logs/app.log"
};
```

Files created:
- `logs/app-2025-11-08.log`
- `logs/app-2025-11-09.log` (after midnight)
- etc.

Rotation happens automatically in background thread with no message loss.

## Performance Optimization

### 1. Preallocate Thread Buffers

Always call `cnanolog_preallocate()` at thread start to avoid first-log allocation overhead.

```c
void* worker_thread(void* arg) {
    cnanolog_preallocate();  // First thing in thread
    // ... rest of thread ...
}
```

Impact: Eliminates ~1-2μs allocation latency on first log.

### 2. CPU Affinity

Pin background writer thread to dedicated core for 3x+ throughput improvement.

```c
cnanolog_init("app.clog");

int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
if (num_cores >= 4) {
    cnanolog_set_writer_affinity(num_cores - 1);
}
```

**Benefits:**
- 3x+ throughput improvement
- Eliminates thread migration overhead (1000-5000 cycles)
- Improved cache locality (L1/L2/L3 stay warm)
- Reduced drop rate under load
- More stable latency

**Best practices:**
- Use last core (num_cores - 1) to avoid OS thread interference
- Requires 4+ cores for best results
- Works on all platforms (Linux, macOS, Windows)

**When to use:**
- High-throughput applications (>10M logs/sec)
- Real-time/low-latency systems
- Multi-core production servers

**When to skip:**
- Low logging volume (<1M logs/sec)
- Resource-constrained environments
- Development/testing environments

### 3. Monitor Statistics

Check drop rate periodically to detect issues:

```c
cnanolog_stats_t stats;
cnanolog_get_stats(&stats);

double drop_rate = (stats.dropped_logs * 100.0) /
                   (stats.total_logs_written + stats.dropped_logs);

if (drop_rate > 1.0) {
    fprintf(stderr, "WARNING: Drop rate %.2f%%\n", drop_rate);
    fprintf(stderr, "Actions: increase buffer size or set CPU affinity\n");
}
```

### 4. Choose Appropriate Output Format

**Use binary mode when:**
- Maximum performance is required
- Disk space is limited
- Post-processing is acceptable
- Logs are archived/analyzed offline

**Use text mode when:**
- Immediate readability is important
- Integration with log aggregation systems (JSON/logfmt)
- tail -f monitoring is needed
- Decompressor tool is unavailable

## Buffer Tuning

### Staging Buffer Size

Default: 8MB per thread. Edit `src/staging_buffer.h`:

```c
#define STAGING_BUFFER_SIZE (8 * 1024 * 1024)
```

**Tuning guidelines:**

**High burst scenarios:**
```c
#define STAGING_BUFFER_SIZE (16 * 1024 * 1024)  // 16MB
// or
#define STAGING_BUFFER_SIZE (32 * 1024 * 1024)  // 32MB
```

Benefits:
- Handles larger burst traffic
- Lower drop rate during spikes
- More headroom for slow background thread

Trade-offs:
- Higher memory usage (per thread)

**Memory-constrained environments:**
```c
#define STAGING_BUFFER_SIZE (4 * 1024 * 1024)   // 4MB
// or
#define STAGING_BUFFER_SIZE (1 * 1024 * 1024)   // 1MB
```

Benefits:
- Lower memory footprint
- More threads possible

Trade-offs:
- Higher drop rate during bursts
- Requires faster background thread (CPU affinity recommended)

**Steady logging (default):**
```c
#define STAGING_BUFFER_SIZE (8 * 1024 * 1024)   // 8MB
```

Balanced for most workloads.

### Maximum Threads

Default: 256 concurrent threads. Edit `src/cnanolog.c`:

```c
#define MAX_STAGING_BUFFERS 256
```

Increase if you need more than 256 logging threads.

### Flush Configuration

Default flush behavior (in `src/cnanolog.c`):

```c
#define FLUSH_BATCH_SIZE 2000        // Flush every N entries
#define FLUSH_INTERVAL_MS 200        // OR flush every N milliseconds
#define BATCH_PROCESS_SIZE 128       // Process N entries per buffer per iteration
```

**FLUSH_BATCH_SIZE:**
- Controls when accumulated logs are written to disk
- Higher = better throughput, higher latency
- Lower = lower latency, more I/O overhead

**FLUSH_INTERVAL_MS:**
- Maximum time logs wait before being flushed
- Prevents log starvation during low activity
- Default 200ms ensures reasonable latency

**BATCH_PROCESS_SIZE:**
- Number of entries processed per buffer before rotating to next buffer
- Higher = better cache locality, potential fairness issues
- Lower = better fairness, more cache misses

## Platform-Specific Notes

### Linux

Full support for all features. Recommended platform for production.

**CPU affinity:**
```c
// Full control via pthread_setaffinity_np()
cnanolog_set_writer_affinity(7);  // Pin to core 7
```

**Isolated cores:**
```bash
# Boot with isolated cores (e.g., cores 6-7)
# Add to kernel command line:
isolcpus=6,7

# Then pin background thread:
cnanolog_set_writer_affinity(7);
```

### macOS

Good support with some limitations.

**CPU affinity:**
```c
// Best-effort via thread_policy_set()
// Still shows 3x improvement in practice
cnanolog_set_writer_affinity(1);
```

**Notes:**
- macOS doesn't support hard CPU pinning
- Affinity is implemented as "affinity tags" (best-effort)
- Performance improvement still significant

### Windows

Full support for all features.

**CPU affinity:**
```c
// Full control via SetThreadAffinityMask()
cnanolog_set_writer_affinity(7);
```

**Platform layer:**
- Uses Windows threads instead of pthreads
- All features work identically

## Integration Examples

### CMake integration

```cmake
find_package(CNanoLog REQUIRED)
target_link_libraries(myapp PRIVATE CNanoLog::cnanolog)
```

### Manual compilation

```bash
gcc -std=c11 -pthread myapp.c -lcnanolog -o myapp
```

### Embedding source files

```bash
# Copy source files
cp -r CNanoLog/src CNanoLog/include myproject/

# Compile together
gcc -std=c11 -pthread \
    myapp.c \
    src/cnanolog.c \
    src/binary_writer.c \
    src/text_formatter.c \
    src/log_registry.c \
    src/staging_buffer.c \
    src/platform.c \
    src/compressor.c \
    src/packer.c \
    -o myapp
```

## Troubleshooting

### High drop rate

**Symptoms:** `stats.dropped_logs` increasing rapidly

**Solutions:**
1. Set CPU affinity: `cnanolog_set_writer_affinity(num_cores - 1)`
2. Increase buffer size in `src/staging_buffer.h`
3. Switch to binary mode for higher throughput
4. Reduce logging frequency in hot paths

### High latency

**Symptoms:** p99.9 latency > 1μs

**Solutions:**
1. Call `cnanolog_preallocate()` at thread start
2. Verify format strings are compile-time literals
3. Check for buffer full conditions (drops)
4. Profile application for other bottlenecks

### File size too large

**Solutions:**
1. Use binary mode for compression (~3.5x smaller)
2. Implement log rotation: `CNANOLOG_ROTATE_DAILY`
3. Reduce debug-level logging in production
4. Archive old logs periodically

### Memory usage too high

**Solutions:**
1. Reduce `STAGING_BUFFER_SIZE` in `src/staging_buffer.h`
2. Fewer logging threads (each thread = 8MB)
3. Switch to binary mode (more efficient compression)

## Best Practices Summary

1. **Always preallocate** in multi-threaded applications
2. **Set CPU affinity** for high-throughput scenarios
3. **Monitor statistics** periodically
4. **Use binary mode** for maximum performance
5. **Enable log rotation** for long-running services
6. **Tune buffer size** based on workload characteristics
7. **Choose appropriate flush parameters** for your latency requirements
