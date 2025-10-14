# CNanoLog Examples

This directory contains comprehensive examples demonstrating all features of CNanoLog.

## Quick Start

### Build All Examples

```bash
cd ../build
cmake ..
make

# Examples will be in the build directory
cd ../build
```

### Run an Example

```bash
./basic_usage
./multithreaded
./high_performance
./statistics_monitoring
./production_server
./error_handling
```

### View Logs

After running an example, decompress the binary logs:

```bash
../tools/decompressor basic_example.clog
../tools/decompressor multithreaded_example.clog | less
../tools/decompressor server.clog | grep ERROR
```

## Examples Overview

### 1. `basic_usage.c` - Getting Started

**What it demonstrates:**
- Logger initialization and shutdown
- Different log levels (info, warn, error, debug)
- Logging with different argument types
- Variadic macros for 0-3 arguments
- Basic statistics

**Best for:** First-time users learning the API

**Run time:** ~1 second

```bash
./basic_usage
```

**Expected output:**
- ~10,000 log entries
- Compression ratio: ~1.5-2x
- Zero dropped logs

---

### 2. `multithreaded.c` - Thread-Safe Logging

**What it demonstrates:**
- Multi-threaded logging (4 worker threads)
- Using `cnanolog_preallocate()` for performance
- Lock-free fast path
- Thread-local staging buffers
- Concurrent logging from multiple threads

**Best for:** Applications using multiple threads

**Run time:** ~5 seconds

```bash
./multithreaded
```

**Key points:**
- Each thread gets its own staging buffer
- Lock-free fast path ensures minimal contention
- Preallocate eliminates first-log overhead
- Background thread processes all buffers

**Expected output:**
- 40,000+ log entries (4 threads × 10K logs each)
- 4 staging buffers active
- Low drop rate

---

### 3. `high_performance.c` - Maximum Performance

**What it demonstrates:**
- CPU affinity for 3x+ performance boost
- Preallocate API usage
- Performance measurement
- Throughput benchmarking
- Drop rate monitoring

**Best for:** Performance-critical applications

**Run time:** ~3 seconds

```bash
./high_performance
```

**Features shown:**
- Automatic core detection
- Writer thread pinning to last core
- Performance before/after comparison
- Throughput measurement (logs/sec)
- Latency calculation (ns/log)

**Expected results:**
- Single-threaded: 50-100M logs/sec
- With affinity: 3x improvement possible
- Latency: 10-20ns per log
- Drop rate: <1%

---

### 4. `statistics_monitoring.c` - Real-Time Monitoring

**What it demonstrates:**
- Real-time statistics monitoring
- Alerting on high drop rates
- Throughput tracking
- Performance diagnostics
- Dedicated monitoring thread

**Best for:** Production environments requiring observability

**Run time:** ~10 seconds (runs 5 monitoring cycles)

```bash
./statistics_monitoring
```

**Monitoring features:**
- Periodic statistics reports (every 2 seconds)
- Drop rate alerts (>1% = alert, >5% = warning)
- Throughput calculation (logs/sec, MB/sec)
- Compression ratio tracking
- Buffer usage monitoring

**Expected output:**
- 5 monitoring reports
- 150K+ logs from 3 worker threads
- Real-time drop rate alerts if needed

---

### 5. `production_server.c` - Complete Production Setup

**What it demonstrates:**
- Complete production-ready setup
- CPU affinity configuration
- Log rotation (size-based)
- Graceful shutdown (SIGINT/SIGTERM)
- Error handling
- Worker threads + monitoring thread

**Best for:** Real-world server applications

**Run time:** Runs until Ctrl+C (or ~30 seconds in demo)

```bash
./production_server
# Press Ctrl+C to stop gracefully
```

**Production features:**
- Signal handler for graceful shutdown
- Automatic log rotation at 100MB
- Real-time monitoring every 5 seconds
- 4 worker threads handling requests
- Simulates real server workload
- Complete error logging

**Simulation:**
- HTTP-style request handling
- Random request types (GET, POST, PUT, DELETE)
- Success rate: ~95%
- Client errors: ~3%
- Server errors: ~2%

**Expected output:**
- Continuous logging until stopped
- Graceful shutdown of all threads
- Final statistics report
- Log rotation if size threshold reached

---

### 6. `error_handling.c` - Robustness & Edge Cases

**What it demonstrates:**
- Proper error handling
- Recovery from errors
- Edge cases and boundary conditions
- Defensive programming practices
- API robustness

**Best for:** Understanding library behavior under edge cases

**Run time:** ~2 seconds

```bash
./error_handling
```

**Tests covered:**
1. Double initialization (should be safe)
2. Logging before initialization (safe, logs dropped)
3. Multiple shutdowns (safe, no-op after first)
4. Invalid file paths (proper error handling)
5. Invalid CPU affinity (parameter validation)
6. Extreme logging (10K rapid logs, long strings)
7. Statistics edge cases (reset, zero logs)
8. Preallocate edge cases (multiple calls)

**Expected results:**
- All error cases handled gracefully
- No crashes or undefined behavior
- Clear error messages where appropriate
- Library continues working after errors

---

## Usage Patterns

### Pattern 1: Simple Application

```c
#include <cnanolog.h>

int main(void) {
    cnanolog_init("app.clog");

    log_info("Application started");
    // ... your code ...
    log_info("Application stopped");

    cnanolog_shutdown();
    return 0;
}
```

**See:** `basic_usage.c`

---

### Pattern 2: Multi-Threaded Application

```c
#include <cnanolog.h>

void* worker(void* arg) {
    cnanolog_preallocate();  // IMPORTANT!

    log_info("Worker started");
    // ... work ...
    log_info("Worker stopped");

    return NULL;
}

int main(void) {
    cnanolog_init("app.clog");
    cnanolog_preallocate();

    // Create threads...
    // Wait for threads...

    cnanolog_shutdown();
    return 0;
}
```

**See:** `multithreaded.c`

---

### Pattern 3: High-Performance Application

```c
#include <cnanolog.h>

int main(void) {
    cnanolog_init("app.clog");

    // Optimize performance
    int num_cores = get_cpu_count();
    cnanolog_set_writer_affinity(num_cores - 1);

    cnanolog_preallocate();

    // ... high-throughput logging ...

    cnanolog_shutdown();
    return 0;
}
```

**See:** `high_performance.c`

---

### Pattern 4: Production Server

```c
#include <cnanolog.h>
#include <signal.h>

volatile int running = 1;

void signal_handler(int sig) {
    running = 0;
}

int main(void) {
    signal(SIGINT, signal_handler);

    cnanolog_init("server.clog");
    setup_cpu_affinity();

    // Start monitoring thread
    // Start worker threads

    while (running) {
        // Server loop...
        check_log_rotation();
    }

    // Graceful shutdown
    cnanolog_shutdown();
    return 0;
}
```

**See:** `production_server.c`

---

## Performance Tips

### 1. Always Preallocate in Threads

```c
void* thread_func(void* arg) {
    cnanolog_preallocate();  // Saves ~292ns on first log
    // ... logging ...
}
```

### 2. Set CPU Affinity for High Throughput

```c
int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
cnanolog_set_writer_affinity(num_cores - 1);  // 3x+ improvement
```

### 3. Monitor Statistics

```c
cnanolog_stats_t stats;
cnanolog_get_stats(&stats);

if (stats.dropped_logs > 0) {
    // Increase buffer size or enable CPU affinity
}
```

### 4. Use Appropriate Log Levels

```c
log_debug(...)  // Development only
log_info(...)   // Normal operations
log_warn(...)   // Warnings
log_error(...)  // Errors only
```

---

## Decompressing Logs

### View All Logs

```bash
../tools/decompressor app.clog
```

### Filter by Level

```bash
../tools/decompressor app.clog | grep ERROR
../tools/decompressor app.clog | grep "WARN\|ERROR"
```

### View Specific Thread

```bash
../tools/decompressor multithreaded_example.clog | grep "Worker 0"
```

### Follow Logs in Real-Time

```bash
tail -f server.clog | ../tools/decompressor -
```

### Save to Text File

```bash
../tools/decompressor app.clog > app.log
less app.log
```

---

## Troubleshooting

### Problem: High Drop Rate

**Solution:**
1. Enable CPU affinity (`cnanolog_set_writer_affinity()`)
2. Increase `STAGING_BUFFER_SIZE` in `src/staging_buffer.h`
3. Reduce logging frequency
4. Use `cnanolog_preallocate()` in all threads

**See:** `statistics_monitoring.c`

---

### Problem: Performance Not as Expected

**Solution:**
1. Verify CPU affinity is set
2. Check that preallocate is called in each thread
3. Monitor drop rate (high drops = performance issue)
4. Ensure -O2 or -O3 optimization flags

**See:** `high_performance.c`

---

### Problem: Logs Not Appearing

**Solution:**
1. Check initialization return value
2. Verify file path is writable
3. Wait for background thread to flush (call `shutdown()`)
4. Check that log level is enabled

**See:** `error_handling.c`

---

## Building Custom Examples

Create your own example:

```c
// my_example.c
#include <cnanolog.h>

int main(void) {
    if (cnanolog_init("my_example.clog") != 0) {
        return 1;
    }

    log_info("My custom example");

    cnanolog_shutdown();
    return 0;
}
```

Compile:

```bash
gcc -o my_example my_example.c \
    -I../include -I../src \
    -L../build -lcnanolog \
    -lpthread
```

Run:

```bash
./my_example
../tools/decompressor my_example.clog
```

---

## Next Steps

1. **Start with `basic_usage.c`** - Learn the fundamentals
2. **Try `multithreaded.c`** - If you have multiple threads
3. **Run `high_performance.c`** - For maximum performance
4. **Study `production_server.c`** - For production deployment
5. **Read `error_handling.c`** - Understand edge cases

## Additional Resources

- **Main README**: `../README.md`
- **API Documentation**: `../include/cnanolog.h`
- **Performance Guide**: `../log/PHASE_6_COMPLETE.md`
- **CPU Affinity Guide**: `../log/CPU_AFFINITY.md`
- **Testing**: `../tests/`

---

**Happy Logging!** 🚀
