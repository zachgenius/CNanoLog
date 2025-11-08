# CNanoLog Benchmarking Guide

## Overview

CNanoLog includes two benchmark programs:

1. **`benchmark_latency`** - Quick latency and throughput tests
2. **`benchmark_comprehensive`** - Comprehensive multi-scale performance testing (small to 10GB+)

## Quick Start

### Run Quick Benchmarks

```bash
# Build
cmake -B build && cmake --build build

# Run quick benchmark
./build/tests/benchmark_latency

# Run comprehensive benchmark (small scales)
./build/tests/benchmark_comprehensive

# Or use convenience script
./scripts/run_benchmarks.sh quick
```

---

## Benchmark Programs

### 1. benchmark_latency

**Purpose**: Quick performance validation

**What it measures**:
- Single-threaded latency (cycles and nanoseconds)
- Different argument types (no args, ints, strings)
- Preallocate API impact
- Throughput (logs/sec)
- Multi-threaded performance (2 and 4 threads)

**Duration**: ~30 seconds

**Usage**:
```bash
./build/tests/benchmark_latency
```

**Example output**:
```
Single-Threaded Latency:
------------------------
  No arguments:         18 cycles (   6.0 ns)
  One integer:          20 cycles (   6.7 ns)
  Two integers:         22 cycles (   7.3 ns)
  Three integers:       24 cycles (   8.0 ns)
  One string:           28 cycles (   9.3 ns)

Throughput (single-threaded):
-----------------------------
  5,000,000 logs in 0.142 seconds
  Throughput: 35.21 million logs/sec
```

---

### 2. benchmark_comprehensive

**Purpose**: Comprehensive performance analysis from small to extreme scales

**What it measures**:
- **Latency distribution**: min/p50/p95/p99/p99.9/max/avg
- **Throughput**: logs/sec and MB/sec
- **File size**: Final log file size
- **Memory usage**: Peak memory consumption
- **Compression ratio**: Effectiveness of compression
- **Drop rate**: Percentage of dropped logs
- **Multi-threaded scaling**: Performance with multiple threads

**Scales**:
| Scale | Logs | Approx Size | Default | Duration |
|-------|------|-------------|---------|----------|
| Tiny | 1,000 | ~10 KB | Yes | <1 sec |
| Small | 10,000 | ~100 KB | Yes | <1 sec |
| Medium | 100,000 | ~1 MB | Yes | 1-2 sec |
| Large | 1,000,000 | ~10 MB | Yes | 5-10 sec |
| XLarge | 10,000,000 | ~100 MB | Yes | 1-2 min |
| Huge | 100,000,000 | ~1 GB | Yes | 10-20 min |
| Extreme | 1,000,000,000 | ~10 GB | No | Hours |

**Usage**:
```bash
# Run all enabled scales (Tiny to Huge)
./build/tests/benchmark_comprehensive

# Run specific scale
./build/tests/benchmark_comprehensive --scale Medium

# Enable extreme scale (10GB+, takes hours!)
./build/tests/benchmark_comprehensive --extreme

# Include multi-threaded tests
./build/tests/benchmark_comprehensive --multithreaded

# Multi-threaded with specific thread count
./build/tests/benchmark_comprehensive --multithreaded --threads 8

# Combination
./build/tests/benchmark_comprehensive --scale Large --multithreaded
```

**Example output**:
```
─────────────────────────────────────────────────────────────────────────────
  Scale: Medium (100,000 logs)
─────────────────────────────────────────────────────────────────────────────
  Time elapsed:        0.003 seconds
  File size:           1.23 MB
  Memory usage:        2.54 GB

  Throughput:
    Logs/sec:          31.25 million
    MB/sec:            385.42 MB/s

  Latency (per log call):
    Min:               0.0 ns
    p50 (median):      20.0 ns
    p95:               42.0 ns
    p99:               83.0 ns
    p99.9:             208.0 ns
    Max:               1250.0 ns
    Average:           24.5 ns

  Compression:         1.65x
  Dropped logs:        0 (0.0000%)
```

---

## Convenience Script

The `run_benchmarks.sh` script provides easy access to common benchmark scenarios:

```bash
# Quick benchmarks (Tiny to Large)
./scripts/run_benchmarks.sh quick

# Full benchmarks (Tiny to Huge)
./scripts/run_benchmarks.sh full

# Extreme scale (includes 10GB+ test)
./scripts/run_benchmarks.sh extreme

# Include multi-threaded tests
./scripts/run_benchmarks.sh full multithreaded

# Specific scale only
./scripts/run_benchmarks.sh specific Medium

# Multi-threaded with 8 threads
./scripts/run_benchmarks.sh full multithreaded --threads 8
```

**Results**: Automatically saved to `benchmark_results/results_<timestamp>.txt`

---

## Understanding Results

### Latency Metrics

**Percentiles explained**:
- **p50 (median)**: 50% of log calls are faster than this
- **p95**: 95% of log calls are faster than this (tail latency)
- **p99**: 99% of log calls are faster than this (important for SLAs)
- **p99.9**: 99.9% of log calls are faster than this (extreme tail latency)

**What to look for**:
- **Median (p50)**: Should be 10-30ns for typical workloads
- **p99**: Should be under 100ns for good performance
- **p99.9**: Spikes here are normal (GC, cache misses, etc.)
- **Max**: Often high due to initialization or background work

### Throughput Metrics

**logs/sec**: Number of log calls per second
- **Single-threaded**: 20-100M logs/sec expected
- **Multi-threaded**: Scales with number of threads

**MB/sec**: File write throughput
- Depends on compression ratio and log size
- 100-500 MB/sec typical

### Drop Rate

**What it means**: Percentage of logs that couldn't be written due to buffer overflow

**Acceptable rates**:
- **0%**: Ideal - no logs lost
- **<0.01%**: Excellent - minimal loss
- **0.01-0.1%**: Good - acceptable for most workloads
- **>1%**: Poor - consider CPU affinity or larger buffers

**How to improve**:
1. Enable CPU affinity: `cnanolog_set_writer_affinity()`
2. Increase `STAGING_BUFFER_SIZE` in `src/staging_buffer.h`
3. Call `cnanolog_preallocate()` in each thread
4. Reduce logging frequency

### Compression Ratio

**Typical values**:
- **1.5-2.0x**: Normal for mixed log types
- **2.0-3.0x**: Good for integer-heavy logs
- **<1.5x**: String-heavy logs

**What affects it**:
- Argument types (integers compress better than strings)
- Value patterns (repeated values compress better)
- Log format complexity

---

## Performance Goals

### Target Metrics (Single-threaded)

| Metric | Target | Excellent |
|--------|--------|-----------|
| p50 latency | <30ns | <20ns |
| p99 latency | <100ns | <50ns |
| Throughput | >20M logs/sec | >50M logs/sec |
| Drop rate | <0.1% | 0% |

### Multi-threaded Scaling

**Expected**:
- 2 threads: 1.8-2.0x single-threaded throughput
- 4 threads: 3.5-4.0x single-threaded throughput
- 8 threads: 6.0-8.0x single-threaded throughput

**Factors**:
- Cache contention
- Memory bandwidth
- CPU affinity configuration

---

## Benchmark Scenarios

### 1. Quick Validation

**Purpose**: Verify basic performance after changes

```bash
./build/tests/benchmark_latency
```

**Expected time**: <1 minute

---

### 2. Comprehensive Single-Threaded

**Purpose**: Understand performance across different scales

```bash
./build/tests/benchmark_comprehensive
```

**Expected time**: ~30 minutes (Tiny to Huge)

**What it shows**:
- How performance scales with log volume
- Where bottlenecks appear
- Memory and file size growth

---

### 3. Multi-Threaded Scaling

**Purpose**: Understand concurrent performance

```bash
./build/tests/benchmark_comprehensive --multithreaded --threads 8
```

**Expected time**: ~45 minutes

**What it shows**:
- Thread scalability
- Contention effects
- Drop rate under high load

---

### 4. Extreme Scale

**Purpose**: Test 10GB+ workloads (production-like)

```bash
./build/tests/benchmark_comprehensive --extreme
```

**Expected time**: Several hours

**What it shows**:
- Sustained performance over time
- Memory stability
- Long-running behavior
- File system performance

**Warning**: This will:
- Generate ~10GB+ of log data
- Take several hours to complete
- Use significant memory

---

## Interpreting Performance

### Good Performance

```
Latency:
  p50:  15-25ns    (Excellent)
  p99:  40-80ns    (Good tail latency)
  p99.9: 150-300ns (Acceptable spikes)

Throughput:
  Single:  40-60M logs/sec   (Great)
  4-thread: 150-200M logs/sec (Excellent scaling)

Drop rate: 0.00%  (Perfect)
```

### Poor Performance

```
Latency:
  p50:  >50ns      (High median)
  p99:  >200ns     (High tail latency)
  p99.9: >1000ns   (Excessive spikes)

Throughput:
  Single:  <10M logs/sec    (Low)
  4-thread: <30M logs/sec   (Poor scaling)

Drop rate: >1%  (Too many drops)
```

### Improvement Checklist

If performance is poor:

1. **Enable CPU affinity**
   ```c
   cnanolog_set_writer_affinity(num_cores - 1);
   ```

2. **Call preallocate in each thread**
   ```c
   cnanolog_preallocate();
   ```

3. **Increase buffer size** (in `src/staging_buffer.h`)
   ```c
   #define STAGING_BUFFER_SIZE (2 * 1024 * 1024)  // 2MB
   ```

4. **Compile with optimizations**
   ```bash
   cmake -DCMAKE_BUILD_TYPE=Release ..
   ```

5. **Check CPU frequency scaling** (Linux)
   ```bash
   cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
   # Should be "performance", not "powersave"
   ```

---

## Comparison with Other Loggers

### Performance Class

**CNanoLog**:
- Latency: 10-30ns (excellent)
- Throughput: 20-100M logs/sec (excellent)
- Class: Ultra-low latency binary logger

**Comparable systems**:
- spdlog: 50-200ns (fast)
- log4cpp: 500-1000ns (moderate)
- printf: 1000-5000ns (slow)

**Use CNanoLog when**:
- Every nanosecond counts
- Million+ logs per second needed
- Binary format is acceptable
- Post-processing is OK

**Use alternatives when**:
- Human-readable logs required immediately
- Low volume logging (<10K logs/sec)
- Text format is mandatory

---

## Troubleshooting

### High Latency

**Symptom**: p99 > 200ns

**Possible causes**:
1. CPU frequency scaling enabled
2. No preallocation in threads
3. High CPU load from other processes
4. Thermal throttling

**Solutions**:
- Disable CPU frequency scaling
- Call `cnanolog_preallocate()`
- Reduce system load
- Check CPU temperature

### High Drop Rate

**Symptom**: >1% drops

**Possible causes**:
1. No CPU affinity set
2. Small staging buffers
3. Very high logging rate
4. Background thread starved

**Solutions**:
- Enable CPU affinity
- Increase `STAGING_BUFFER_SIZE`
- Reduce logging frequency
- Use Release build

### Low Throughput

**Symptom**: <10M logs/sec single-threaded

**Possible causes**:
1. Debug build (not Release)
2. Slow CPU
3. High drop rate
4. System overloaded

**Solutions**:
- Use Release build (`-DCMAKE_BUILD_TYPE=Release`)
- Upgrade hardware
- Address drop rate issues
- Reduce background load

---

## Automated Benchmarking

### CI/CD Integration

```bash
# Run quick benchmark in CI
./scripts/run_benchmarks.sh quick > benchmark_results.txt

# Check for regressions
python scripts/check_performance.py benchmark_results.txt
```

### Performance Regression Detection

Create `scripts/check_performance.py`:
```python
import sys

def check_benchmark(filename):
    with open(filename) as f:
        content = f.read()

    # Check for drop rate
    if "Drop rate:" in content:
        # Extract and check drop rate
        # Fail if >0.1%
        pass

    # Check latency
    # Fail if p99 >200ns

    return 0  # Pass

if __name__ == "__main__":
    sys.exit(check_benchmark(sys.argv[1]))
```

---

## Summary

**Quick validation**:
```bash
./build/tests/benchmark_latency  # 30 seconds
```

**Comprehensive testing**:
```bash
./scripts/run_benchmarks.sh full multithreaded  # 30-60 minutes
```

**Production simulation**:
```bash
./scripts/run_benchmarks.sh extreme  # Several hours
```

**Results location**: `benchmark_results/results_<timestamp>.txt`

---

For more information:
- **Implementation**: `tests/benchmark_comprehensive.c`
- **Performance tuning**: `log/CPU_AFFINITY.md`
- **Architecture**: `README.md`
