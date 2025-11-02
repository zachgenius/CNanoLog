# CNanoLog

**Ultra-fast, low-latency binary logging library for C**

CNanoLog is a high-performance logging library inspired by [NanoLog](https://github.com/PlatformLab/NanoLog) from Stanford. It achieves **54ns median, 108ns p99.9 latency** on HFT hardware, outperforming both NanoLog and fmtlog across all workloads.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C11](https://img.shields.io/badge/std-C11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey.svg)](https://github.com)

## Performance Highlights

**Comprehensive Benchmark Results (Production Linux with Core Binding)**

### Single-Threaded Performance (Timestamps OFF)

| Metric | CNanoLog | NanoLog | fmtlog | spdlog | glog |
|--------|----------|---------|---------|--------|------|
| **0 args p50** | **54ns** 🥇 | 108ns | 108ns | 1134ns | 2484ns |
| **2 ints p99.9** | **108ns** 🥇 | 702ns | 702ns | 22788ns | 9936ns |
| **string p99.9** | **108ns** 🥇 | 702ns | 702ns | 24408ns | 10098ns |
| **medium msg p99.9** | **594ns** 🥇 | 1512ns | 810ns | 23112ns | 13230ns |

### Multithreaded Performance (4 threads, core binding)

| Library | p50 | p99.9 | Result |
|---------|-----|-------|--------|
| **CNanoLog (TS OFF)** | **108ns** | **162ns** | 🥇 **Fastest** |
| **CNanoLog (TS ON)** | 162ns | 216ns | 🥈 Excellent |
| fmtlog | 162ns | 216ns | 🥈 Excellent |
| NanoLog | 162ns | 1080ns | Fair |
| spdlog | 4752ns | 54540ns | 40x slower |
| glog | 4266ns | 56970ns | 35x slower |

**Key Results**:
- 🏆 **CNanoLog wins all single-threaded benchmarks**
- ⚡ **String operations**: Improved to **108ns p99.9** (best-in-class)
- 🚀 **2.5x faster than NanoLog** for medium messages (594ns vs 1512ns)
- 🔥 **6.7x faster than NanoLog** in multithreaded p99.9 (162ns vs 1080ns)
- 💾 **Smallest log files**: 43MB vs NanoLog 263MB, spdlog 482MB
- ⚡ **20-200x faster than spdlog/glog** across all workloads

📊 **[Complete Performance Analysis](docs/PERFORMANCE.md)** - Deep dive with comprehensive comparison table of all 5 libraries

## Key Features

### Ultra-Low Latency
- **54ns median, 108ns p99.9** latency on HFT hardware
- **Lock-free producer threads** using thread-local staging buffers
- **Binary encoding** with deferred formatting (no sprintf overhead)
- **Cache-line aligned** buffers prevent false sharing
- **Preallocate API** to eliminate first-log allocation overhead

### Binary Logging Format
- **Compact binary encoding** with variable-byte integer compression
- **Deferred formatting** - format strings stored in dictionary, not in log stream
- **Efficient decompression** tool to convert binary logs to human-readable text
- **Optional nanosecond-precision timestamps** using high-resolution CPU counters (can be disabled at compile-time)

### Thread Safety
- **Lock-free fast path** for producer threads
- **Thread-local staging buffers** eliminate contention
- **Cache-line alignment** prevents false sharing
- **Tested with 40+ concurrent threads** under stress

### Production Ready
- **Automatic log rotation** by date with seamless file transitions
- **Real-time statistics API** for monitoring (<5% overhead)
- **CPU affinity control** to pin background thread to specific core (3x+ performance boost)
- **Graceful overflow handling** with configurable drop policy
- **Comprehensive test suite** with 98% coverage
- **Cross-platform support**: Linux, macOS, Windows (GCC, Clang, MSVC)
- **Zero external dependencies**

## Inspiration

CNanoLog is inspired by the [NanoLog](https://github.com/PlatformLab/NanoLog) paper and implementation from Stanford University:

> **NanoLog: A Nanosecond Scale Logging System**
> Stephen Yang and Seo Jin Park and John Ousterhout
> USENIX ATC 2018

Key concepts adapted from NanoLog:
- **Compile-time format string extraction** to minimize hot-path work
- **Binary logging format** with deferred formatting
- **Dictionary-based decompression** for efficient decoding
- **Variable-byte integer compression** to reduce log file size

CNanoLog reimagines these concepts for pure C (C11), focusing on simplicity, portability, cross platform, and zero dependencies.

## Quick Start

### Basic Usage

```c
#include <cnanolog.h>

int main(void) {
    // Initialize logger
    if (cnanolog_init("app.clog") != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    // Log at different levels
    log_info("Application started");
    log_warn("Low memory: %d MB available", available_mb);
    log_error("Connection failed: %s (code: %d)", error_msg, error_code);

    // Log with multiple arguments
    log_info3("Position: x=%d y=%d z=%d", x, y, z);

    // Clean shutdown
    cnanolog_shutdown();
    return 0;
}
```

### Decompressing Binary Logs

```bash
# Decompress binary log to human-readable text (default format)
./decompressor app.clog > app.log

# Custom output format
./decompressor -f "%t | %l | %m" app.clog

# CSV format for analysis
./decompressor -f "%t,%l,%f,%L,%m" app.clog app.csv

# View specific log level
./decompressor app.clog | grep ERROR

# Show help and format options
./decompressor --help
```

**Format Tokens:**
- `%t` - Human-readable timestamp (YYYY-MM-DD HH:MM:SS.nnnnnnnnn)
- `%T` - Raw timestamp (CPU ticks)
- `%r` - Relative time since start (seconds)
- `%l` - Log level (INFO, WARN, ERROR, DEBUG)
- `%f` - Source filename
- `%L` - Line number
- `%m` - Formatted log message
- `%%` - Literal % character

### Multi-Threaded Usage

```c
#include <cnanolog.h>
#include <pthread.h>

void* worker_thread(void* arg) {
    // Preallocate staging buffer to avoid first-log overhead
    cnanolog_preallocate();

    // Log from thread (lock-free fast path)
    log_info1("Worker %d started", thread_id);

    // ... work ...

    log_info1("Worker %d finished", thread_id);
    return NULL;
}

int main(void) {
    cnanolog_init("multi.clog");

    pthread_t threads[4];
    for (int i = 0; i < 4; i++) {
        pthread_create(&threads[i], NULL, worker_thread, &i);
    }

    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }

    cnanolog_shutdown();
    return 0;
}
```

### Automatic Log Rotation

```c
#include <cnanolog.h>

int main(void) {
    // Configure daily log rotation
    cnanolog_rotation_config_t config = {
        .policy = CNANOLOG_ROTATE_DAILY,
        .base_path = "logs/app.clog"
    };

    // Initialize with rotation
    if (cnanolog_init_ex(&config) != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    // Log messages as usual - rotation happens automatically
    log_info("Application started with daily rotation");

    // Files are created with dates:
    // logs/app-2025-11-02.clog
    // logs/app-2025-11-03.clog (after midnight)
    // logs/app-2025-11-04.clog (after next midnight)

    cnanolog_shutdown();
    return 0;
}
```

**Features:**
- ✅ **Automatic rotation** at midnight (local time)
- ✅ **Dated filenames** (e.g., app-2025-11-02.clog)
- ✅ **Seamless transitions** - no message loss
- ✅ **Complete files** - each file has its own dictionary
- ✅ **Zero overhead** - rotation happens in background thread

### Production Monitoring

```c
#include <cnanolog.h>

void check_logging_health(void) {
    cnanolog_stats_t stats;
    cnanolog_get_stats(&stats);

    printf("Logs written: %llu\n", stats.total_logs_written);
    printf("Logs dropped: %llu\n", stats.dropped_logs);
    printf("Bytes written: %llu\n", stats.total_bytes_written);
    printf("Compression: %.2fx\n", stats.compression_ratio_x100 / 100.0);

    // Alert if drop rate > 1%
    double drop_rate = (stats.dropped_logs * 100.0) /
                       (stats.total_logs_written + stats.dropped_logs);
    if (drop_rate > 1.0) {
        fprintf(stderr, "WARNING: High drop rate: %.2f%%\n", drop_rate);
    }
}
```

### High-Performance Setup with CPU Affinity

For maximum performance, pin the background writer thread to a dedicated CPU core:

```c
#include <cnanolog.h>
#include <unistd.h>  // For sysconf()

int main(void) {
    // Initialize logger
    if (cnanolog_init("app.clog") != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    // Pin background writer to last CPU core
    // This prevents thread migration and improves cache locality
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cores >= 4) {
        int target_core = num_cores - 1;  // Use last core

        if (cnanolog_set_writer_affinity(target_core) == 0) {
            printf("✓ Writer thread pinned to core %d\n", target_core);
            printf("  Expected benefits: 2-3x throughput improvement\n");
        }
    }

    // Preallocate for main thread
    cnanolog_preallocate();

    // Application code with optimized logging
    log_info("Application started with optimized logging");

    cnanolog_shutdown();
    return 0;
}
```

**Benefits of CPU Affinity:**
- 🎯 **Eliminates thread migration** overhead (~1000-5000 cycles)
- 📊 **Improved cache locality** (L1/L2/L3 caches stay warm)
- 📉 **Reduced drop rate** (measured: 52% → 0% under load)
- 🔒 **More predictable latency** (lower variance)

**Platform Support:**
- ✅ Linux: Full support via `pthread_setaffinity_np()`
- ⚠️ macOS: Best-effort via `thread_policy_set()` (still shows 3x improvement)
- ✅ Windows: Full support via `SetThreadAffinityMask()`

## Building

### Requirements
- C11 compiler (GCC, Clang, or MSVC)
- CMake 3.10+ (for build system)
- POSIX threads (pthreads) or Windows threads

### Build Instructions

```bash
# Clone repository
git clone https://github.com/zachgenius/CNanoLog.git
cd CNanoLog

# Create build directory
mkdir build && cd build

# Configure and build (default: with timestamps)
cmake ..
make

# Or build in extreme performance mode (no timestamps)
cmake -DCNANOLOG_ENABLE_TIMESTAMPS=OFF ..
make

# Run tests
make test

# Run benchmarks
./benchmark_latency
```

**CMake Options:**
- `CNANOLOG_ENABLE_TIMESTAMPS` - Enable/disable high-resolution timestamps (default: ON)
- `BUILD_EXAMPLES` - Build example programs (default: ON)
- `BUILD_TESTS` - Build test suite (default: ON)

### Integration Options

**Option 1: Single-Header File (Easiest)**
```bash
# Generate single-header file
cd CNanoLog
make single-header  # or run: ./tools/generate_single_header.sh

# Copy the generated cnanolog.h to your project
cp cnanolog.h your_project/
```

Then in your code:
```c
// In ONE .c file, define the implementation:
#define CNANOLOG_IMPLEMENTATION
#include "cnanolog.h"

// In all other files, just include normally:
#include "cnanolog.h"
```

Compile with:
```bash
gcc -std=c11 -pthread myapp.c -o myapp
```

**Option 2: Compiled Library**
```bash
# Build and install
cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..
make
sudo make install

# Link in your project
gcc myapp.c -lcnanolog -o myapp
```

**Option 3: Source Integration**
```bash
# Copy source files into your project
cp -r src include your_project/

# Compile with your project
gcc myapp.c cnanolog.c binary_writer.c log_registry.c \
    staging_buffer.c background_thread.c -o myapp
```

**Option 4: Package Managers (vcpkg / Conan)**

CNanoLog supports both vcpkg and Conan for easy cross-platform installation:

**vcpkg:**
```bash
# Install via vcpkg (once published)
vcpkg install cnanolog

# Use in your CMake project
find_package(CNanoLog CONFIG REQUIRED)
target_link_libraries(myapp PRIVATE CNanoLog::cnanolog)
```

**Conan:**
```bash
# Install via Conan (once published)
conan install --requires=cnanolog/1.0.0

# conanfile.txt
[requires]
cnanolog/1.0.0

[generators]
CMakeDeps
CMakeToolchain
```

For detailed integration instructions:
- **vcpkg**: See [VCPKG.md](docs/VCPKG.md)
- **Conan**: See [CONAN.md](docs/CONAN.md)

## Documentation

- 📊 **[Performance Analysis](docs/PERFORMANCE.md)** - Detailed comparison with NanoLog and fmtlog
- 📖 **[Binary Format Specification](docs/BINARY_FORMAT_SPEC.md)** - Complete format documentation
- 🚀 **[Optimization Guide](docs/OPTIMIZATION_GUIDE.md)** - Performance tuning tips
- 🔧 **[Build Instructions](docs/BUILD.md)** - Detailed build and integration guide

## Architecture

### Binary Log Format

CNanoLog uses a compact binary format to minimize logging overhead:

```
[Header: Magic + Version]
[Dictionary: Log Sites with Format Strings]
[Log Entries: Compressed Binary Data]
```

**Log Entry Structure:**
```
[Log ID: 4 bytes]
[Timestamp: 8 bytes]
[Arguments: Variable-byte encoded]
```

**Variable-Byte Integer Encoding:**
- 1-byte values: stored in 1 byte
- 2-byte values: stored in 2 bytes
- 4-byte values: stored in 4 bytes
- 8-byte values: stored in 8 bytes
- Strings: length prefix + content

### Thread Architecture

```
┌─────────────────┐
│ Producer Thread │
│  (Lock-free)    │
└────────┬────────┘
         │
         v
┌─────────────────┐
│ Staging Buffer  │  <-- Thread-local, cache-line aligned
│  (16 KB)        │
└────────┬────────┘
         │
         v
┌─────────────────┐
│ Background      │  <-- Single consumer thread
│ Thread          │
└────────┬────────┘
         │
         v
┌─────────────────┐
│ Binary Log File │
└─────────────────┘
```

## API Reference

### Initialization
```c
// Basic initialization (no rotation)
int cnanolog_init(const char* log_file);

// Advanced initialization with rotation
typedef enum {
    CNANOLOG_ROTATE_NONE = 0,    // No rotation (default)
    CNANOLOG_ROTATE_DAILY,        // Rotate at midnight
} cnanolog_rotation_policy_t;

typedef struct {
    cnanolog_rotation_policy_t policy;  // Rotation policy
    const char* base_path;              // Base path (e.g., "logs/app.clog")
} cnanolog_rotation_config_t;

int cnanolog_init_ex(const cnanolog_rotation_config_t* config);

// Shutdown and cleanup
void cnanolog_shutdown(void);

// Thread optimization
void cnanolog_preallocate(void);  // Call at thread start
```

### Logging Macros
```c
log_debug(format, ...)           // Debug level
log_info(format, ...)            // Info level
log_warn(format, ...)            // Warning level
log_error(format, ...)           // Error level

// Optimized variants for specific argument counts
log_info1(format, arg1)
log_info2(format, arg1, arg2)
log_info3(format, arg1, arg2, arg3)
```

### Statistics API
```c
typedef struct {
    uint64_t total_logs_written;
    uint64_t total_bytes_written;
    uint64_t dropped_logs;
    uint64_t compression_ratio_x100;
    uint64_t staging_buffers_active;
    uint64_t background_wakeups;
} cnanolog_stats_t;

void cnanolog_get_stats(cnanolog_stats_t* stats);
void cnanolog_reset_stats(void);
```

### CPU Affinity API
```c
// Pin background writer thread to specific CPU core
// Returns 0 on success, -1 on failure (non-fatal)
int cnanolog_set_writer_affinity(int core_id);
```

**Benefits:**
- 3x+ throughput improvement
- Eliminates thread migration overhead (1000-5000 cycles)
- Improved cache locality (L1/L2/L3 stay warm)
- More predictable latency

**Usage:**
```c
cnanolog_init("app.clog");

// Pin to last core (typically core N-1)
int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
cnanolog_set_writer_affinity(num_cores - 1);
```

**Platform Support:**
- Linux: Full (pthread_setaffinity_np)
- macOS: Best-effort (thread_policy_set)
- Windows: Full (SetThreadAffinityMask)

## Optimisations

**Recommendation:** Always call `cnanolog_preallocate()` at thread start for predictable latency.

### CPU Affinity Impact

Pinning the background writer thread to a dedicated core provides significant performance benefits:


**Benefits:**
- 3x+ throughput improvement
- Eliminates dropped logs under load
- More stable latency (reduced variance)
- Better cache locality

**Best Practices:**
- Use last core (e.g., core N-1) to avoid OS threads
- Works even on "unsupported" platforms (macOS showed 3x improvement)
- Combine with isolated cores on Linux for maximum performance

```c
// Recommended setup for high-performance logging
cnanolog_init("app.clog");
int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
cnanolog_set_writer_affinity(num_cores - 1);  // Pin to last core
```

### Extreme Performance Mode (No Timestamps)

For maximum throughput when timestamps aren't needed:

**Benefits:**
- ~43% smaller log entries (14 → 6 bytes)
- Eliminates rdtsc() overhead (~5-10ns per log)
- More logs fit in staging buffers
- Faster initialization (no calibration)

**When to use:**
- High-frequency logging where timing isn't critical
- Event counting/aggregation scenarios
- Memory-constrained embedded systems
- Maximum throughput benchmarking

**Build command:**
```bash
cmake -DCNANOLOG_ENABLE_TIMESTAMPS=OFF ..
make
```

The decompressor automatically handles both timestamp and no-timestamp log files.

## Testing

CNanoLog has **98% test coverage** with comprehensive unit, integration, and stress tests.

### Run All Tests

```bash
cd build

# Run all tests
make test

# Or run individually
./test_format
./test_binary_writer
./test_compression
./test_e2e
./test_binary_integration
./test_registry
./test_statistics
./test_cacheline_alignment
./test_multithreaded_stress
./test_buffer_overflow
./benchmark_latency
./benchmark_comprehensive
```

### Test Coverage

- ✅ Format string parsing
- ✅ Binary serialization
- ✅ Compression/decompression
- ✅ Multi-threaded logging (40+ threads)
- ✅ Buffer overflow and recovery
- ✅ Statistics accuracy
- ✅ Cache-line alignment
- ✅ End-to-end integration

## Configuration

### Compile-Time Options

**Timestamp Control (Extreme Performance Mode)**

Disable timestamps for maximum throughput when you don't need precise timing:

```bash
# Build without timestamps (extreme performance mode)
cmake -DCNANOLOG_ENABLE_TIMESTAMPS=OFF ..
make

# Build with timestamps (default - high-resolution rdtsc)
cmake -DCNANOLOG_ENABLE_TIMESTAMPS=ON ..
make
```

Benefits of disabling timestamps:
- **~43% smaller log entries** (14 bytes → 6 bytes per entry header)
- **No rdtsc() overhead** (~5-10ns saved per log)
- **Faster startup** (skips 100ms timestamp calibration)
- **More logs fit in buffers** (less memory pressure)

The decompressor automatically detects whether the log file has timestamps.

**Buffer Size Tuning**

```c
// src/staging_buffer.h
#define STAGING_BUFFER_SIZE (8 * 1024 * 1024)  // 8 MB per thread (default)

// Adjust based on your workload:
// - High burst scenarios: Increase to 16MB or 32MB
// - Many threads, memory constrained: Decrease to 4MB or 1MB
// - Steady logging: 8MB is optimal
```

### Runtime Options

```c
// Drop policy (current: drop on full)
// Future: Blocking mode, custom handlers

// Log levels (compile-time filtering)
#define CNANOLOG_MIN_LEVEL CNANOLOG_LEVEL_INFO
```

## Best Practices

### 1. Preallocate at Thread Start
```c
void* worker_thread(void* arg) {
    cnanolog_preallocate();  // Avoid first-log allocation overhead
    // ... logging ...
    return NULL;
}
```

### 2. Set CPU Affinity for High Performance
```c
// Pin background writer to dedicated core for 3x+ performance boost
cnanolog_init("app.clog");

#ifdef __linux__
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cores >= 4) {
        // Use last core to avoid OS/application thread contention
        cnanolog_set_writer_affinity(num_cores - 1);
    }
#elif defined(__APPLE__)
    // macOS: best-effort, but still shows 3x improvement
    cnanolog_set_writer_affinity(1);
#elif defined(_WIN32)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    if (sysinfo.dwNumberOfProcessors >= 4) {
        cnanolog_set_writer_affinity(sysinfo.dwNumberOfProcessors - 1);
    }
#endif
```

**When to use:**
- ✅ High-throughput workloads (>10M logs/sec)
- ✅ Multi-core systems (4+ cores)
- ✅ Real-time/low-latency applications
- ❌ Low logging volume (<1M logs/sec)
- ❌ Resource-constrained environments

### 3. Monitor Statistics
```c
// Check drop rate periodically
cnanolog_stats_t stats;
cnanolog_get_stats(&stats);
if (stats.dropped_logs > 0) {
    // Consider increasing buffer size or setting CPU affinity
}
```

### 4. Use Appropriate Log Levels
```c
log_debug(...)  // Development only (can be compiled out)
log_info(...)   // Normal operations
log_warn(...)   // Warnings
log_error(...)  // Errors and critical events
```

### 5. Use Automatic Log Rotation
```c
// Recommended: Use automatic daily rotation
cnanolog_rotation_config_t config = {
    .policy = CNANOLOG_ROTATE_DAILY,
    .base_path = "logs/app.clog"
};
cnanolog_init_ex(&config);

// Files automatically rotate at midnight:
// logs/app-2025-11-02.clog
// logs/app-2025-11-03.clog
// etc.

// Each file is complete and can be decompressed independently
```

## Limitations

- **Maximum 16 arguments** per log call (configurable via CNANOLOG_MAX_ARGS)
- **Binary format** requires decompressor tool (not human-readable)
- **Drop policy** - logs dropped when buffer full (alternative: blocking mode not yet implemented)
- **Single output file** - multiple outputs not yet supported (network, syslog, etc.)
- **Format string constraints** - must be compile-time string literals
- **Date-based rotation only** - size-based rotation not yet implemented

## Roadmap

### Completed ✅
- [x] Phase 1: Basic logging infrastructure
- [x] Phase 2: Format string parsing & registry
- [x] Phase 3: Binary format & serialization
- [x] Phase 4: Compression & decompression
- [x] Phase 5: High-resolution timestamps
- [x] Phase 6: Optimization & polishing
- [x] Phase 7: Testing & validation
- [x] Phase 8: CPU affinity support for 3x+ performance boost
- [x] Single-header version for easy integration
- [x] Benchmarking suite for performance regression (ref: [benchmark](https://github.com/zachgenius/cnanolog_benchmark))
- [x] Extreme performance mode (no timestamps) for maximum throughput
- [x] Decompressor with custom output formats (CSV, JSON, etc.)
- [x] Automatic log rotation by date

### Future Enhancements
- [ ] Burst scenario optimizations (Latency spikes)
- [ ] Plain text logging mode (human-readable without decompressor)
- [ ] Size-based log rotation (in addition to date-based)
- [ ] Multiple log outputs (file + network)
- [ ] Log filtering by level or category
- [ ] Blocking mode (alternative to drop policy)
- [ ] Async flush control
- [ ] Windows-specific optimizations
- [ ] Custom compression algorithms

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Add tests for new functionality
4. Ensure all tests pass
5. Submit a pull request

## License

MIT License - see [LICENSE](LICENSE) file for details.

## Acknowledgments

- **NanoLog** (Stanford) - Original inspiration and design concepts
- **spdlog** - Reference for C++ logging best practices
- **Claude Code** - AI-assisted code generation and documentation (no doubt XD)

## Contact

- **Issues**: [GitHub Issues](https://github.com/zachgenius/CNanoLog/issues)
- **Discussions**: [GitHub Discussions](https://github.com/zachgenius/CNanoLog/discussions)

**CNanoLog** - Ultra-fast logging for performance-critical C applications.
