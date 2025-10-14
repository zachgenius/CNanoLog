# Comprehensive Logging Library Benchmark - Testing Plan

## Executive Summary

This document outlines a comprehensive benchmarking methodology to compare CNanoLog against industry-leading logging libraries: NanoLog, spdlog, Boost.Log, and glog.

**Goal:** Provide fair, reproducible, and meaningful performance comparisons across multiple dimensions.

---

## 1. Libraries Under Test

### 1.1 Target Libraries

| Library | Version | Language | Key Features |
|---------|---------|----------|--------------|
| **CNanoLog** | 1.0.0 | C | Binary format, lock-free, compile-time optimization |
| **NanoLog** | Latest | C++ | Binary format, extremely low latency, compile-time |
| **spdlog** | 1.x | C++ | Header-only, fast, popular, multiple sinks |
| **Boost.Log** | 1.x | C++ | Feature-rich, flexible, part of Boost |
| **glog** | Latest | C++ | Google's logging library, production-proven |

### 1.2 Library Configurations

Each library will be tested in multiple configurations:

**CNanoLog:**
- With timestamps (default)
- Without timestamps (fast path)
- CPU affinity enabled/disabled
- Buffer sizes: 1MB, 4MB, 8MB

**NanoLog:**
- Standard configuration
- With/without timestamps

**spdlog:**
- Async mode (thread pool)
- Sync mode
- Different queue sizes

**Boost.Log:**
- Async sink
- Sync sink

**glog:**
- Standard configuration
- Async mode if available

---

## 2. Benchmark Methodology

### 2.1 NanoLog's Methodology (Reference)

NanoLog's benchmark approach:
1. **Hot path measurement**: Measure only the logging call (NANO_LOG macro)
2. **Exclude background work**: Compression and I/O are asynchronous
3. **Exclude setup costs**: Buffer allocation, registration are one-time
4. **Timestamp handling**: Optional, measured separately
5. **CPU pinning**: Pin threads to specific cores for consistency
6. **Warmup**: Run warmup iterations before measurement

**Key insight:** NanoLog measures the **application-facing latency** (time until control returns to application), not end-to-end latency.

### 2.2 Industry Standard Methodologies

**Google's Benchmark Guidelines:**
- Measure multiple iterations (>1000)
- Report percentiles (p50, p95, p99, p99.9, max)
- Control for CPU frequency scaling
- Minimize system noise
- Use statistical methods to validate results

**LMAX Disruptor Benchmarks:**
- Measure both throughput and latency
- Test under different load conditions
- Measure at various concurrency levels
- Report drop/discard rates

**Systems Performance (Brendan Gregg):**
- USE method: Utilization, Saturation, Errors
- Measure resource consumption (CPU, memory, I/O)
- Compare apples-to-apples (same hardware, same conditions)

### 2.3 Our Approach: Multi-Dimensional Benchmark

We'll measure **5 key dimensions**:

1. **Latency** (hot path, application-facing)
2. **Throughput** (logs/second)
3. **Resource Usage** (CPU, memory, I/O)
4. **Reliability** (drop rates, data loss)
5. **Scalability** (multi-threaded performance)

---

## 3. Test Scenarios

### 3.1 Single-Threaded Tests

**Scenario ST-1: Baseline Latency**
- **Workload:** 1M logs, single thread, no contention
- **Measurement:** Latency distribution (p50, p95, p99, p99.9, max)
- **Purpose:** Measure best-case latency

**Scenario ST-2: Sustained Throughput**
- **Workload:** Log at maximum rate for 10 seconds
- **Measurement:** Logs/second, drop rate
- **Purpose:** Find maximum sustainable throughput

**Scenario ST-3: Burst Performance**
- **Workload:** Idle → 100K logs burst → idle (repeat 10x)
- **Measurement:** Latency during burst, recovery time
- **Purpose:** Test buffer handling under burst load

**Scenario ST-4: Variable Log Sizes**
- **Workload:** Mix of small (1 arg), medium (4 args), large (8 args) logs
- **Measurement:** Latency vs message complexity
- **Purpose:** Real-world message patterns

### 3.2 Multi-Threaded Tests

**Scenario MT-1: Concurrent Producers (Low Contention)**
- **Workload:** 4 threads, 100K logs each
- **Measurement:** Aggregate throughput, per-thread latency
- **Purpose:** Measure scalability with moderate concurrency

**Scenario MT-2: Concurrent Producers (High Contention)**
- **Workload:** 16 threads, 500K logs each
- **Measurement:** Throughput, drop rate, latency degradation
- **Purpose:** Stress test under high contention

**Scenario MT-3: Mixed Read/Write**
- **Workload:** 8 producer threads + 2 consumer threads (stats/monitoring)
- **Measurement:** Interference between logging and stats
- **Purpose:** Real-world scenario with monitoring

**Scenario MT-4: Thread Scaling**
- **Workload:** Vary threads: 1, 2, 4, 8, 16, 32
- **Measurement:** Throughput scaling curve
- **Purpose:** Find optimal thread count

### 3.3 Stress Tests

**Scenario S-1: Memory Pressure**
- **Workload:** Log 10M entries with limited memory
- **Measurement:** Memory usage peak, swap usage
- **Purpose:** Memory efficiency

**Scenario S-2: Disk I/O Bottleneck**
- **Workload:** Log to slow disk (simulated with rate limiting)
- **Measurement:** Buffer saturation, drop rate
- **Purpose:** Behavior under I/O constraints

**Scenario S-3: Long Duration**
- **Workload:** Log continuously for 1 hour
- **Measurement:** Performance stability over time
- **Purpose:** Memory leak detection, sustained performance

**Scenario S-4: Recovery from Overload**
- **Workload:** Overload → normal → measure recovery time
- **Measurement:** Time to clear backlog
- **Purpose:** Production resilience

### 3.4 Production Simulation

**Scenario P-1: Web Server**
- **Workload:** Request/response pattern, bursty traffic
- **Pattern:** 50 req/sec baseline, 5000 req/sec peaks
- **Measurement:** p99 latency during peak
- **Purpose:** Web service scenario

**Scenario P-2: Trading System**
- **Workload:** Critical path logging, ultra-low latency required
- **Pattern:** Occasional high-priority logs
- **Measurement:** Max latency, jitter
- **Purpose:** Real-time systems

**Scenario P-3: Data Pipeline**
- **Workload:** High sustained volume, batch processing
- **Pattern:** Steady 5M logs/sec
- **Measurement:** Throughput stability, resource usage
- **Purpose:** Analytics/ETL scenario

---

## 4. Metrics & Measurements

### 4.1 Latency Metrics

**Application-Facing Latency (Hot Path):**
- Time from log call start to return
- Measured using RDTSC for nanosecond precision
- Report: min, p50, p95, p99, p99.9, max

**End-to-End Latency:**
- Time from log call to data on disk
- Measured with flush markers
- Report: median, p99

**Timestamp Collection:**
```c
uint64_t start = rdtsc();
LOG_FUNCTION("message", args...);
uint64_t end = rdtsc();
latency_ns = (end - start) * 1e9 / cpu_frequency;
```

### 4.2 Throughput Metrics

**Logs per Second:**
```
throughput = total_logs / elapsed_time_seconds
```

**Megabytes per Second:**
```
MB_per_sec = (total_bytes_written / (1024*1024)) / elapsed_time
```

**Effective Throughput (accounting for drops):**
```
effective_throughput = logs_written / total_logs_attempted
```

### 4.3 Resource Usage Metrics

**CPU Utilization:**
- Per-thread CPU usage
- Total system CPU for logging
- Background thread CPU

**Memory Usage:**
- Peak RSS (Resident Set Size)
- Buffer memory allocated
- Memory per thread

**I/O:**
- Write IOPS
- Write bandwidth (MB/s)
- fsync frequency

### 4.4 Reliability Metrics

**Drop Rate:**
```
drop_rate = (dropped_logs / total_attempted) * 100
```

**Data Loss:**
- Logs attempted vs logs in file (after decompression)
- Corruption detection

**Availability:**
- Uptime during stress tests
- Recovery time after overload

---

## 5. Test Environment

### 5.1 Hardware Requirements

**Minimum Specification:**
- CPU: Intel/AMD x86_64, 8+ cores
- RAM: 16GB
- Disk: SSD (NVMe preferred)
- OS: Linux (Ubuntu 20.04+), GCC 11+

**Ideal Specification:**
- CPU: Intel Xeon/AMD EPYC, 16+ cores
- RAM: 64GB
- Disk: NVMe SSD (dedicated for logging)
- OS: Ubuntu 22.04, GCC 13

### 5.2 System Configuration

**CPU Isolation:**
```bash
# Isolate cores for benchmarking
isolcpus=6,7,8,9,10,11,12,13,14,15
nohz_full=6,7,8,9,10,11,12,13,14,15
```

**Disable CPU frequency scaling:**
```bash
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo performance > $cpu
done
```

**Disable transparent huge pages:**
```bash
echo never > /sys/kernel/mm/transparent_hugepage/enabled
```

**Increase file descriptor limit:**
```bash
ulimit -n 65536
```

### 5.3 Compilation Settings

**Optimization Level:**
- All libraries: `-O3 -march=native -DNDEBUG`
- Link-time optimization: `-flto`
- Same compiler version for all

**Fair Comparison Rules:**
1. Same compiler and flags for all libraries
2. Same hardware and OS configuration
3. Same log message formats (where possible)
4. Same background thread configuration
5. Warmup runs before measurement

---

## 6. Implementation Plan

### 6.1 Benchmark Structure

```
benchmark/
├── TESTING_PLAN.md          # This document
├── README.md                # Quick start guide
├── CMakeLists.txt           # Build configuration
├── common/
│   ├── benchmark_utils.h    # Shared utilities
│   ├── timing.h             # High-precision timing
│   ├── stats.h              # Statistical analysis
│   └── formatters.h         # Result formatting
├── configs/
│   ├── cnanolog.conf        # CNanoLog configuration
│   ├── nanolog.conf         # NanoLog configuration
│   ├── spdlog.conf          # spdlog configuration
│   ├── boost.conf           # Boost.Log configuration
│   └── glog.conf            # glog configuration
├── scenarios/
│   ├── single_threaded.cpp  # ST-1 to ST-4
│   ├── multi_threaded.cpp   # MT-1 to MT-4
│   ├── stress_tests.cpp     # S-1 to S-4
│   └── production_sim.cpp   # P-1 to P-3
├── libraries/
│   ├── cnanolog_bench.c     # CNanoLog adapter
│   ├── nanolog_bench.cpp    # NanoLog adapter
│   ├── spdlog_bench.cpp     # spdlog adapter
│   ├── boost_bench.cpp      # Boost.Log adapter
│   └── glog_bench.cpp       # glog adapter
├── scripts/
│   ├── setup_env.sh         # Environment setup
│   ├── run_all.sh           # Run all benchmarks
│   ├── run_suite.sh         # Run specific suite
│   ├── compare.py           # Generate comparison report
│   └── visualize.py         # Generate graphs
└── results/
    ├── raw/                 # Raw JSON results
    ├── reports/             # Markdown reports
    └── graphs/              # Performance graphs
```

### 6.2 Benchmark Adapter Interface

Each library will implement a common interface:

```c
// benchmark_adapter.h
typedef struct {
    const char* name;
    const char* version;

    // Lifecycle
    int (*init)(const char* config_file);
    void (*shutdown)(void);

    // Logging
    void (*log_with_0_args)(const char* msg);
    void (*log_with_1_int)(const char* fmt, int arg);
    void (*log_with_2_ints)(const char* fmt, int a1, int a2);
    void (*log_with_string)(const char* fmt, const char* str);

    // Statistics
    void (*get_stats)(bench_stats_t* stats);
    void (*reset_stats)(void);

    // Configuration
    void (*set_async)(int enabled);
    void (*set_buffer_size)(size_t bytes);
    void (*set_cpu_affinity)(int core);
} benchmark_adapter_t;
```

### 6.3 Timeline

**Week 1: Infrastructure**
- ✅ Design testing plan (this document)
- Create common utilities
- Set up build system
- Implement timing infrastructure

**Week 2: Library Adapters**
- Implement CNanoLog adapter
- Implement NanoLog adapter
- Implement spdlog adapter
- Implement Boost.Log adapter
- Implement glog adapter

**Week 3: Scenario Implementation**
- Single-threaded scenarios (ST-1 to ST-4)
- Multi-threaded scenarios (MT-1 to MT-4)
- Stress tests (S-1 to S-4)
- Production simulations (P-1 to P-3)

**Week 4: Testing & Analysis**
- Run full benchmark suite
- Collect results
- Statistical analysis
- Generate reports and graphs
- Write summary document

---

## 7. Reporting Format

### 7.1 Raw Data Format (JSON)

```json
{
  "benchmark": "ST-1-baseline-latency",
  "library": "cnanolog",
  "version": "1.0.0",
  "config": {
    "threads": 1,
    "iterations": 1000000,
    "buffer_size": 8388608,
    "cpu_affinity": true
  },
  "system": {
    "cpu": "Intel Xeon E5-2680 v4",
    "cores": 16,
    "ram_gb": 64,
    "os": "Ubuntu 22.04"
  },
  "results": {
    "latency_ns": {
      "min": 10.2,
      "p50": 19.8,
      "p95": 39.6,
      "p99": 50.4,
      "p999": 630.0,
      "max": 213810.0,
      "mean": 24.6,
      "stddev": 145.3
    },
    "throughput": {
      "logs_per_sec": 29440000,
      "mb_per_sec": 703.27,
      "effective_rate": 1.0
    },
    "resources": {
      "cpu_percent": 145.6,
      "rss_mb": 34.38,
      "io_write_mb": 23.89
    },
    "reliability": {
      "drop_rate": 0.0,
      "drops": 0,
      "total": 1000000
    }
  }
}
```

### 7.2 Summary Report Format

**Markdown Table:**
```markdown
## Scenario ST-1: Baseline Latency (1M logs, single-threaded)

| Library | p50 (ns) | p99 (ns) | p99.9 (ns) | Throughput (M/s) | Drop Rate |
|---------|----------|----------|------------|------------------|-----------|
| CNanoLog | 19.8 | 50.4 | 630 | 29.4 | 0.00% |
| NanoLog | 7.0 | 15.2 | 250 | 80.5 | 0.00% |
| spdlog | 120.5 | 450.3 | 1200 | 8.3 | 0.00% |
| Boost.Log | 850.2 | 2400.8 | 5600 | 1.2 | 0.00% |
| glog | 310.4 | 980.6 | 2100 | 3.2 | 0.05% |
```

**Winner Analysis:**
- Lowest latency: NanoLog (7.0ns p50)
- Best throughput: NanoLog (80.5M/s)
- Most reliable: CNanoLog, NanoLog, spdlog (0% drops)

### 7.3 Visualization

**Graphs to generate:**
1. Latency distribution (CDF plot)
2. Throughput comparison (bar chart)
3. Scaling curve (line graph, threads vs throughput)
4. Resource usage (stacked bar chart)
5. Drop rate heatmap (threads × load)

---

## 8. Validation & Verification

### 8.1 Result Validation

**Statistical Significance:**
- Run each test 5 times
- Report mean ± standard deviation
- Use t-test to validate differences (p < 0.05)

**Data Integrity:**
- Verify logged data matches expected count
- Decompress and validate content
- Check for corruption

**Reproducibility:**
- Document exact versions, commits
- Provide Docker container for reproduction
- Share raw data and scripts

### 8.2 Sanity Checks

**Before each run:**
- ✅ CPU frequency locked to max
- ✅ No other processes running
- ✅ Disk space available (>100GB)
- ✅ Memory sufficient (no swap)
- ✅ Network disabled (no interference)

**During run:**
- Monitor CPU temperature (prevent throttling)
- Check for kernel messages (dmesg)
- Verify disk not full

**After run:**
- Validate result files exist and are complete
- Check for anomalies (outliers > 3σ)
- Compare against baseline

---

## 9. Benchmark Acceptance Criteria

### 9.1 Success Criteria

**Completeness:**
- ✅ All 16 scenarios implemented
- ✅ All 5 libraries tested
- ✅ Results reproducible (±5% variance)

**Fairness:**
- ✅ Same hardware and OS for all
- ✅ Same compiler flags for all
- ✅ Documented configuration differences
- ✅ Apples-to-apples comparisons

**Rigor:**
- ✅ Statistical validation
- ✅ Multiple runs per test
- ✅ Peer review of methodology
- ✅ Results match expected ranges

### 9.2 Known Limitations

**Timestamp Handling:**
- NanoLog: Timestamps optional
- CNanoLog: Timestamps always on
- spdlog: Timestamps always on
- Must compare fairly (with vs without)

**Async vs Sync:**
- Some libraries default to sync
- Some default to async
- Test both modes where applicable

**Platform Differences:**
- Linux-specific optimizations may not apply to macOS/Windows
- Document platform-specific results separately

---

## 10. Next Steps

### 10.1 Immediate Actions

1. **Set up benchmark infrastructure**
   - Create directory structure
   - Implement common utilities
   - Build system configuration

2. **Implement CNanoLog adapter**
   - Reference implementation
   - Validate against known results

3. **Create single baseline test**
   - ST-1: Baseline latency
   - Verify methodology works

4. **Iterate on remaining libraries**
   - One library at a time
   - Compare against CNanoLog baseline

### 10.2 Future Enhancements

**Extended Testing:**
- Cross-platform testing (macOS, Windows)
- ARM64 architecture testing
- Different CPU vendors (Intel vs AMD)

**Additional Libraries:**
- quill (another low-latency logger)
- log4cpp
- g3log

**Advanced Scenarios:**
- Distributed logging
- Log rotation performance
- Compression benchmarks

---

## 11. References

**NanoLog Paper:**
- "NanoLog: A Nanosecond Scale Logging System" (2018)
- URL: https://www.usenix.org/system/files/conference/atc18/atc18-yang.pdf

**Industry Standards:**
- LMAX Disruptor Performance Testing
- Google Benchmark Framework
- Systems Performance by Brendan Gregg

**Related Work:**
- spdlog benchmarks: https://github.com/gabime/spdlog#benchmarks
- Boost.Log documentation
- glog GitHub repository

---

## Appendix A: Quick Start

```bash
# Clone and setup
git clone https://github.com/yourusername/cnanolog
cd cnanolog/benchmark

# Install dependencies
./scripts/setup_env.sh

# Build all benchmarks
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run baseline test
./build/scenarios/single_threaded --library=cnanolog --scenario=ST-1

# Run full suite
./scripts/run_all.sh

# Generate report
./scripts/compare.py results/raw/*.json > results/reports/comparison.md
```

---

## Appendix B: Configuration Examples

### CNanoLog Configuration (cnanolog.conf)
```ini
[general]
buffer_size = 8388608  # 8MB
cpu_affinity = 15      # Pin to core 15
timestamps = true

[async]
flush_batch_size = 500
flush_interval_ms = 50
```

### spdlog Configuration (spdlog.conf)
```ini
[async]
queue_size = 8192
thread_count = 1
overflow_policy = block
```

---

**Document Version:** 1.0
**Date:** 2025-01-14
**Author:** CNanoLog Team
**Status:** Draft for Review
