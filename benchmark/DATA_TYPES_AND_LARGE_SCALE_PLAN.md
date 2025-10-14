# Data Types and Large-Scale Testing - Comprehensive Plan

**Created:** 2025-01-14
**Status:** Implementation Complete, Testing Pending

---

## 📋 Executive Summary

This document describes the implementation of two major benchmark enhancements:
1. **ST-4: Data type performance testing** - Tests 11 different data types to identify performance characteristics
2. **Large-scale testing** - Validates performance with 100M+ logs (up to 1 billion logs / ~5GB)

Both features are **fully implemented and built**, ready for testing.

---

## ✅ What Has Been Implemented

### 1. Enhanced Benchmark Adapter Interface

**File:** `benchmark/common/benchmark_adapter.h`

**New Methods Added:**
```c
// Extended argument counts
void (*log_8_ints)(const char* fmt, int a1, ..., int a8);

// Various data types
void (*log_1_long)(const char* fmt, long arg);
void (*log_1_uint64)(const char* fmt, uint64_t arg);
void (*log_1_float)(const char* fmt, float arg);
void (*log_1_double)(const char* fmt, double arg);
void (*log_1_string)(const char* fmt, const char* str);

// Mixed types
void (*log_mixed)(const char* fmt, int i1, const char* s1, int i2);
void (*log_mixed2)(const char* fmt, int i1, double d1, const char* s1);
```

**Purpose:** Provides a standard interface for testing different data types across all logging libraries.

---

### 2. CNanoLog Adapter Implementation

**File:** `benchmark/libraries/cnanolog_adapter.c`

**Implemented Functions:**
- All 11 data type methods properly mapped to CNanoLog's `log_info1()`, `log_info3()`, `log_info8()` macros
- Full C11 `_Generic` type detection support (already existed in CNanoLog)
- Automatic type inference for int, long, uint64, float, double, string, mixed types

**Key Features:**
- Uses CNanoLog's native type detection system
- Zero overhead - types detected at compile time
- Binary format preserves type information

---

### 3. ST-4: Data Type Performance Scenario

**File:** `benchmark/scenarios/single_threaded.c`

**Test Configuration:**
- **Scale:** 100K logs per data type (11 types total = 1.1M logs)
- **Warmup:** 1K logs per type
- **Measurement:** Full latency histogram per type

**Data Types Tested:**
1. **1 int** - Baseline single integer
2. **2 ints** - Most common case (used in ST-1)
3. **4 ints** - Multiple arguments
4. **8 ints** - Maximum argument count
5. **1 long** - 64-bit signed integer
6. **1 uint64** - 64-bit unsigned integer
7. **1 float** - Single-precision floating point
8. **1 double** - Double-precision floating point
9. **1 string** - String argument (const char*)
10. **mixed (int+string+int)** - Mixed type combination
11. **mixed2 (int+double+string)** - Complex mixed types

**Output Metrics:**
- Latency p50, p99, max for each type
- Throughput (M logs/sec) for each type
- Comparison: fastest vs slowest type
- Performance range (latency spread)

**Example Output:**
```
Testing different data types (100000 logs per type)...
─────────────────────────────────────────────────────────────────────────
  1 int                        p50:    18.5 ns  p99:     45.2 ns  max:    1250.0 ns  30.12 M/s
  2 ints                       p50:    19.8 ns  p99:     50.4 ns  max:    1380.0 ns  29.44 M/s
  4 ints                       p50:    21.2 ns  p99:     55.8 ns  max:    1520.0 ns  27.85 M/s
  8 ints                       p50:    24.5 ns  p99:     68.3 ns  max:    1890.0 ns  24.21 M/s
  1 long                       p50:    18.8 ns  p99:     46.5 ns  max:    1280.0 ns  29.88 M/s
  1 uint64                     p50:    18.9 ns  p99:     47.1 ns  max:    1295.0 ns  29.76 M/s
  1 float                      p50:    19.2 ns  p99:     48.8 ns  max:    1340.0 ns  29.34 M/s
  1 double                     p50:    19.4 ns  p99:     49.5 ns  max:    1365.0 ns  29.12 M/s
  1 string                     p50:    22.8 ns  p99:     62.5 ns  max:    1650.0 ns  26.15 M/s
  mixed (int+string+int)       p50:    23.5 ns  p99:     65.8 ns  max:    1720.0 ns  25.45 M/s
  mixed2 (int+double+string)   p50:    24.1 ns  p99:     68.9 ns  max:    1785.0 ns  24.88 M/s

Performance across 11 data types:
─────────────────────────────────────────────────────────────────────────
  Fastest:  1 int at 18.5 ns (p50)
  Slowest:  8 ints at 24.5 ns (p50)
  Range:    6.0 ns (1.3x)
```

---

### 4. Large-Scale Testing Framework

**File:** `benchmark/scenarios/large_scale.c`

**Scales Supported:**
| Scale | Logs        | Approx Size | Use Case                    |
|-------|-------------|-------------|-----------------------------|
| 5M    | 5,000,000   | ~25 MB      | Quick validation            |
| 10M   | 10,000,000  | ~50 MB      | Small-scale stress          |
| 50M   | 50,000,000  | ~250 MB     | Medium-scale stress         |
| 100M  | 100,000,000 | ~500 MB     | Large-scale validation      |
| 200M  | 200,000,000 | ~1 GB       | Extended stress             |
| 500M  | 500,000,000 | ~2.5 GB     | Extreme scale               |
| 1B    | 1,000,000,000| ~5 GB      | Maximum scale (1B logs!)    |

**Key Features:**

1. **Sampled Latency Measurement:**
   - Measures latency every N logs (e.g., every 5000 for 100M scale)
   - Avoids memory overhead of storing 100M+ latency samples
   - Still provides accurate percentile statistics

2. **Progress Reporting:**
   - Reports progress every N logs (e.g., every 20M for 100M scale)
   - Shows current throughput, drop rate, completion percentage
   - Helps monitor long-running tests

3. **Adaptive Wait Times:**
   - Waits for background writer based on scale
   - 100M+ logs: 5 second wait
   - 500M+ logs: 10 second wait
   - Ensures complete flush before measurement

4. **Comprehensive Metrics:**
   - Total duration (seconds)
   - Sustained throughput (M logs/sec)
   - Latency distribution (p50, p99, p99.9, max)
   - Drop rate and total drops
   - File size (MB)
   - Memory usage (KB)

**Example Output:**
```
╔═══════════════════════════════════════════════════════════════════════════╗
║ Large-Scale Test: 100M logs                                              ║
╚═══════════════════════════════════════════════════════════════════════════╝

Initializing CNanoLog...
Running benchmark (100000000 logs, sampling every 5000)...
─────────────────────────────────────────────────────────────────────────
  20000000 / 100000000 logs (20.0%)  |  28.45 M/s  |  drops: 0.0000%
  40000000 / 100000000 logs (40.0%)  |  29.12 M/s  |  drops: 0.0000%
  60000000 / 100000000 logs (60.0%)  |  28.88 M/s  |  drops: 0.0000%
  80000000 / 100000000 logs (80.0%)  |  29.34 M/s  |  drops: 0.0000%
  100000000 / 100000000 logs (100.0%)  |  29.01 M/s  |  drops: 0.0000%
─────────────────────────────────────────────────────────────────────────
Logging complete. Waiting for background writer to flush...

Results for CNanoLog:
─────────────────────────────────────────────────────────────────────────
  Scale:         100M logs
  Duration:      3.45 seconds

  Throughput:
    29.01 M logs/sec
    145.05 MB/sec

  Latency (sampled):
    p50:    19.8 ns
    p99:    52.3 ns
    p99.9:  680.5 ns
    max:    15240.0 ns

  Reliability:
    Drop rate: 0.0000%
    Dropped:   0 / 100000000

  Resources:
    File size:  500 MB
    Memory:     34580 KB
─────────────────────────────────────────────────────────────────────────

✅ EXCELLENT: 0% drop rate
```

---

### 5. Build System Updates

**File:** `benchmark/CMakeLists.txt`

**Changes:**
- Added `large_scale` executable target
- Linked all necessary libraries (cnanolog_adapter, bench_common)
- Platform-specific linking (pthread, m) for both executables
- Updated installation targets
- Updated build instructions in CMake output

**Built Executables:**
- `benchmark/build/single_threaded` - Runs ST-1 and ST-4 scenarios
- `benchmark/build/large_scale` - Runs large-scale tests (5M to 1B logs)

---

## 📖 Usage Guide

### Prerequisites

1. **Build the benchmark suite:**
   ```bash
   cd /Users/zach/Develop/CNanoLog/benchmark
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```

2. **Optional: Setup environment for accurate benchmarks:**
   ```bash
   ./scripts/setup_env.sh  # Or: sudo ./scripts/setup_env.sh
   ```

### Running ST-4: Data Type Tests

**Basic usage:**
```bash
./build/single_threaded --library=cnanolog --scenario=ST-4
```

**What it does:**
- Tests 11 different data types
- 100K logs per type (1.1M total)
- Compares latency and throughput across types
- Identifies which data types are fastest/slowest

**Expected runtime:** ~10-15 seconds

**Use cases:**
- Understand performance characteristics of different log argument types
- Optimize application by choosing faster data types
- Validate that CNanoLog handles all types efficiently
- Compare against other libraries (when adapters implemented)

---

### Running Large-Scale Tests

**Quick validation (5M logs):**
```bash
./build/large_scale --library=cnanolog --scale=5M
```
- **Runtime:** ~0.2 seconds
- **Use case:** Quick smoke test

**Standard validation (100M logs):**
```bash
./build/large_scale --library=cnanolog --scale=100M
```
- **Runtime:** ~3-5 seconds
- **Use case:** Standard large-scale validation
- **File size:** ~500 MB

**Extreme stress test (1B logs):**
```bash
./build/large_scale --library=cnanolog --scale=1B
```
- **Runtime:** ~30-40 seconds
- **Use case:** Maximum scale stress test
- **File size:** ~5 GB
- **⚠️ Warning:** Ensure you have 6+ GB free disk space

**All available scales:**
```bash
./build/large_scale --library=cnanolog --scale=<SCALE>

# Where <SCALE> is one of:
# 5M, 10M, 50M, 100M, 200M, 500M, 1B
```

**Command-line options:**
```bash
./build/large_scale --help

Options:
  --library <name>    Library to benchmark (default: cnanolog)
  --scale <name>      Scale to test (default: 100M)
  --help              Show this help
```

---

## 🧪 Testing Plan

### Phase 1: Data Type Validation

**Test ST-4 with different configurations:**

1. **Baseline test:**
   ```bash
   ./build/single_threaded --library=cnanolog --scenario=ST-4
   ```
   - Verify all 11 data types complete successfully
   - Check that latency differences are reasonable (1-2x range)
   - Confirm 0% drop rate

2. **Expected results:**
   - Simple types (int, long, uint64) should be fastest (~18-20ns)
   - Complex types (8 ints, mixed) should be slower (~23-25ns)
   - Strings may be slightly slower due to length calculation
   - All drop rates should be 0.0000%

3. **Validation criteria:**
   - ✅ All 11 types complete without errors
   - ✅ p50 latency: 18-25ns range
   - ✅ p99 latency: <100ns
   - ✅ Drop rate: 0%
   - ✅ Throughput: 24-30 M logs/sec

### Phase 2: Large-Scale Validation

**Progressive scale testing:**

1. **5M scale (warm-up):**
   ```bash
   ./build/large_scale --library=cnanolog --scale=5M
   ```
   - Quick smoke test
   - Verify output format
   - Check file size (~25 MB)

2. **100M scale (primary validation):**
   ```bash
   ./build/large_scale --library=cnanolog --scale=100M
   ```
   - Standard large-scale test
   - Verify sustained throughput remains consistent
   - Check progress reporting
   - Measure total runtime

3. **1B scale (stress test):**
   ```bash
   ./build/large_scale --library=cnanolog --scale=1B
   ```
   - Maximum scale validation
   - Ensure no memory leaks
   - Verify drop rate remains low
   - Check file size (~5 GB)

**Validation criteria per scale:**

| Scale | Expected Runtime | Expected Throughput | Max Drop Rate | File Size  |
|-------|------------------|---------------------|---------------|------------|
| 5M    | <0.5s           | 25-30 M/s           | 0%            | ~25 MB     |
| 10M   | <1s             | 25-30 M/s           | 0%            | ~50 MB     |
| 50M   | ~2s             | 25-30 M/s           | <0.1%         | ~250 MB    |
| 100M  | 3-5s            | 25-30 M/s           | <0.1%         | ~500 MB    |
| 200M  | 6-10s           | 25-30 M/s           | <0.5%         | ~1 GB      |
| 500M  | 15-20s          | 25-30 M/s           | <1%           | ~2.5 GB    |
| 1B    | 30-40s          | 25-30 M/s           | <2%           | ~5 GB      |

### Phase 3: Performance Analysis

**Compare data types:**
1. Run ST-4 multiple times (3-5 runs)
2. Extract p50 latency for each data type
3. Calculate average and standard deviation
4. Create ranking: fastest → slowest

**Analyze large-scale behavior:**
1. Plot throughput over time (from progress reports)
2. Check if throughput degrades over time
3. Verify latency remains consistent at all scales
4. Measure memory usage growth

**Document findings:**
- Which data types are most efficient?
- Does latency increase with scale?
- What is the practical maximum scale (0% drops)?
- How does throughput compare to ST-1 baseline?

---

## 📊 Expected Performance Characteristics

### Data Types (ST-4)

**Predicted ranking (fastest to slowest):**
1. **1 int** - Simplest case, baseline
2. **1 long / 1 uint64** - Same complexity as int
3. **1 float / 1 double** - Floating point, minimal overhead
4. **2 ints** - Current benchmark standard
5. **4 ints** - Multiple arguments, still fast
6. **1 string** - String length calculation overhead
7. **mixed (int+string+int)** - Type mixing overhead
8. **8 ints** - Maximum arguments, more data to copy
9. **mixed2 (int+double+string)** - Complex mixed types

**Expected latency range:** 18ns (fastest) to 25ns (slowest) = ~1.4x

### Large-Scale Tests

**Expected behavior:**

1. **Throughput consistency:**
   - Should remain ~25-30 M logs/sec across all scales
   - Minor degradation acceptable for 1B scale (<10%)

2. **Latency stability:**
   - p50 should remain ~19-20ns regardless of scale
   - p99 may increase slightly with scale (up to 2x)
   - max latency spikes expected (OS scheduler)

3. **Drop rates:**
   - 0% for 5M-100M scales (fits in 8MB buffer)
   - <0.5% for 200M-500M scales (sustained load)
   - <2% for 1B scale (extreme sustained load)

4. **Memory usage:**
   - Should remain constant (~34-40 MB RSS)
   - Staging buffers: 8MB per thread = 8MB total
   - Background writer overhead: ~5-10 MB

---

## 🔍 Troubleshooting

### Issue: High drop rates in ST-4

**Symptoms:** Drop rate >1% for any data type

**Possible causes:**
- Buffer size too small (unlikely with 8MB)
- CPU affinity not enabled
- Background writer not keeping up

**Solutions:**
1. Check CPU affinity is enabled (last core)
2. Increase buffer size in `src/staging_buffer.h`
3. Adjust flush policy (FLUSH_BATCH_SIZE, FLUSH_INTERVAL_MS)

### Issue: Large-scale test hangs or crashes

**Symptoms:** Test stops responding, segfault, or runs out of memory

**Possible causes:**
- Insufficient disk space
- Memory leak in histogram allocation
- OS resource limits

**Solutions:**
1. Check free disk space: `df -h /tmp`
2. Monitor memory: `top` or `htop` during test
3. Start with smaller scale (5M, 10M) to isolate issue

### Issue: Inconsistent performance across runs

**Symptoms:** Latency varies significantly between runs

**Possible causes:**
- CPU frequency scaling enabled
- Background processes consuming CPU
- Thermal throttling

**Solutions:**
1. Run `setup_env.sh` to lock CPU frequency
2. Close unnecessary applications
3. Ensure system is cool (check CPU temp)
4. Run multiple times and average results

---

## 🚀 Future Enhancements

### Short-term (1-2 weeks)

1. **Multi-threaded data type testing:**
   - Test data types under contention
   - Measure scalability of each type

2. **String length variation:**
   - Test short strings (10 bytes)
   - Test medium strings (100 bytes)
   - Test long strings (1KB)

3. **Automated reporting:**
   - Generate CSV/JSON output
   - Create comparison tables
   - Plot latency distributions

### Medium-term (2-4 weeks)

4. **Cross-library comparison:**
   - Implement NanoLog adapter
   - Implement spdlog adapter
   - Run ST-4 and large-scale for all libraries

5. **Burst mixed with data types:**
   - Test which data types handle bursts best
   - Measure drop rates per type under stress

6. **Memory profiling:**
   - Track memory usage over time
   - Detect memory leaks at large scales

### Long-term (1-2 months)

7. **Visualization dashboard:**
   - Real-time throughput graphs
   - Latency heatmaps
   - Drop rate monitoring

8. **Production simulation:**
   - Mix different data types (realistic workload)
   - Variable log rates
   - Bursty patterns

9. **Optimization guide:**
   - Recommend optimal data types
   - Tuning guide based on results
   - Performance prediction model

---

## 📝 File Summary

### New Files Created

1. **`benchmark/scenarios/large_scale.c`** (340 lines)
   - Large-scale testing implementation
   - Scales: 5M to 1B logs
   - Progress reporting and metrics

2. **`benchmark/DATA_TYPES_AND_LARGE_SCALE_PLAN.md`** (this file)
   - Comprehensive documentation
   - Usage guide
   - Testing plan

### Modified Files

1. **`benchmark/common/benchmark_adapter.h`**
   - Added 7 new data type methods
   - Extended interface for comprehensive testing

2. **`benchmark/libraries/cnanolog_adapter.c`**
   - Implemented all new data type methods
   - Proper mapping to CNanoLog macros

3. **`benchmark/scenarios/single_threaded.c`**
   - Added ST-4 data type scenario (185 lines)
   - Tests 11 different data types
   - Comparison and ranking

4. **`benchmark/CMakeLists.txt`**
   - Added large_scale executable
   - Updated build instructions
   - Platform-specific linking

5. **`benchmark/IMPLEMENTATION_STATUS.md`**
   - Updated to reflect new features
   - Added ST-4 and large-scale to completed features

6. **`tools/CMakeLists.txt`**
   - Fixed PROJECT_SOURCE_DIR for subdirectory builds
   - Enables benchmark build as subdirectory

---

## ✅ Checklist

### Implementation ✅
- [x] Extended benchmark adapter interface
- [x] Implemented CNanoLog adapter methods
- [x] Created ST-4 data type scenario
- [x] Created large-scale testing framework
- [x] Updated build system
- [x] Built all executables successfully
- [x] Updated documentation

### Testing 🔲
- [ ] Run ST-4 baseline test
- [ ] Validate all 11 data types
- [ ] Run 5M scale test
- [ ] Run 100M scale test
- [ ] Run 1B scale test (optional)
- [ ] Verify drop rates are acceptable
- [ ] Document performance characteristics

### Analysis 🔲
- [ ] Compare data type latencies
- [ ] Analyze throughput consistency
- [ ] Measure memory usage
- [ ] Create performance report
- [ ] Identify optimization opportunities

---

## 🎯 Next Steps

### Immediate (this session)
1. Review this plan with user
2. Get approval to proceed with testing
3. Run ST-4 to validate data types
4. Run 100M large-scale test

### Short-term (next session)
1. Complete testing of all scales
2. Document results in TESTING_RESULTS.md
3. Create comparison tables
4. Identify any issues or optimizations

### Medium-term (next week)
1. Implement other library adapters (NanoLog, spdlog)
2. Run cross-library comparison
3. Generate comprehensive benchmark report
4. Publish results

---

**Status:** ✅ **Ready for Testing**

All code is implemented, built, and ready to run. Executables are in `benchmark/build/`:
- `single_threaded` - For ST-4 data type tests
- `large_scale` - For 100M+ log tests

**Estimated testing time:**
- ST-4: 10-15 seconds
- 100M scale: 3-5 seconds
- Full suite (all scales): 1-2 minutes
