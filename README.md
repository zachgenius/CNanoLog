# CNanoLog

Ultra-fast, low-latency logging library for C with nanosecond-scale overhead.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C11](https://img.shields.io/badge/std-C11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey.svg)](https://github.com)

## Overview

CNanoLog achieves **54ns median, 108ns p99.9 latency** on production hardware by moving expensive formatting work to a background thread. Producers only pack raw arguments into thread-local buffers using lock-free operations.

**Performance vs. Popular Logging Libraries:**

| Library | p50 Latency | p99.9 Latency | Result |
|---------|-------------|---------------|--------|
| CNanoLog | 54ns | 108ns | Fastest |
| NanoLog (C++) | 108ns | 702ns | 6.5x slower |
| fmtlog (C++) | 108ns | 702ns | 6.5x slower |
| spdlog | 1134ns | 22788ns | 211x slower |

See [PERFORMANCE.md](docs/PERFORMANCE.md) for comprehensive benchmarks.

## Key Features

- **Low latency**: 54ns median logging overhead
- **Lock-free producers**: Thread-local staging buffers eliminate contention
- **Flexible output**: Binary (compressed) or text (human-readable) formats
- **Custom patterns**: JSON, logfmt, syslog, or any custom format
- **Automatic rotation**: Daily log rotation with seamless transitions
- **Zero dependencies**: Standard C11 only
- **Cross-platform**: Linux, macOS, Windows

## Quick Start

```c
#include <cnanolog.h>

int main(void) {
    // Initialize logger
    cnanolog_init("app.clog");

    // Log messages (supports 0-50 arguments automatically)
    LOG_INFO("Application started");
    LOG_WARN("Low memory: %d MB available", available_mb);
    LOG_ERROR("Connection failed: %s (code: %d)", error_msg, code);

    // Cleanup
    cnanolog_shutdown();
    return 0;
}
```

**Compile:**
```bash
gcc -std=c11 -pthread myapp.c -lcnanolog -o myapp
```

**Decompress binary logs:**
```bash
./decompressor app.clog > app.log
```

## Installation

### Build from source

```bash
git clone https://github.com/zachgenius/CNanoLog.git
cd CNanoLog
mkdir build && cd build
cmake ..
make
sudo make install
```

### CMake options

- `CNANOLOG_ENABLE_TIMESTAMPS` - Enable timestamps (default: ON)
- `BUILD_EXAMPLES` - Build examples (default: ON)
- `BUILD_TESTS` - Build tests (default: ON)

## Documentation

- [Usage Guide](docs/USAGE.md) - Detailed usage examples and patterns
- [API Reference](docs/API.md) - Complete API documentation
- [Configuration](docs/CONFIGURATION.md) - Tuning and optimization options
- [Performance](docs/PERFORMANCE.md) - Benchmarks and comparisons
- [Binary Format](docs/BINARY_FORMAT_SPEC.md) - Binary log format specification

## Basic Usage Examples

### Text mode with custom format

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

### Daily log rotation

```c
cnanolog_rotation_config_t config = {
    .policy = CNANOLOG_ROTATE_DAILY,
    .base_path = "logs/app.log",
    .format = CNANOLOG_OUTPUT_TEXT
};
cnanolog_init_ex(&config);

// Logs rotate automatically at midnight:
// logs/app-2025-11-08.log
// logs/app-2025-11-09.log
```

### Multi-threaded logging

```c
void* worker_thread(void* arg) {
    cnanolog_preallocate();  // Avoid first-log allocation
    LOG_INFO("Worker %d started", thread_id);
    // ... work ...
    return NULL;
}

int main(void) {
    cnanolog_init("app.clog");

    pthread_t threads[4];
    for (int i = 0; i < 4; i++)
        pthread_create(&threads[i], NULL, worker_thread, &i);

    for (int i = 0; i < 4; i++)
        pthread_join(threads[i], NULL);

    cnanolog_shutdown();
}
```

## Text Format Patterns

Supported tokens for custom text output:

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

**Examples:**

```c
// JSON
.text_pattern = "{\"time\":\"%t\",\"level\":\"%l\",\"msg\":\"%m\"}"

// Logfmt
.text_pattern = "time=\"%t\" level=%l file=%f:%n msg=\"%m\""

// Syslog
.text_pattern = "%d %T myapp[%l]: %m"
```

## Architecture

```
Producer Thread (Lock-free)
    ↓
Thread-local Staging Buffer (8MB)
    ↓
Background Thread
    ↓ (format/compress)
Log File (binary or text)
```

Producers pack raw arguments into binary format (~54ns). Background thread handles formatting, compression, and I/O, keeping producer latency low.

## Performance Notes

- **Producer latency is identical** for binary and text modes
- Text mode: ~5-8M logs/sec throughput
- Binary mode: ~50M logs/sec throughput
- Use `cnanolog_preallocate()` in threads for predictable latency
- Pin background thread to dedicated core for 3x throughput boost

## Limitations

- Maximum 50 arguments per log call
- Format strings must be compile-time literals
- Logs dropped when buffer full (no blocking mode yet)
- Date-based rotation only (no size-based rotation)

## License

MIT License - see [LICENSE](LICENSE) file for details.

## Acknowledgments

Inspired by [NanoLog](https://github.com/PlatformLab/NanoLog) from Stanford University (USENIX ATC 2018).

## Links

- [GitHub Repository](https://github.com/zachgenius/CNanoLog)
- [Issue Tracker](https://github.com/zachgenius/CNanoLog/issues)
- [Benchmark Repository](https://github.com/zachgenius/cnanolog_benchmark)
