# CNanoLog Comprehensive Benchmark Suite

A fair, rigorous, and reproducible benchmark comparing CNanoLog with industry-leading logging libraries.

## Quick Start

```bash
# 1. Setup environment
cd benchmark
./scripts/setup_env.sh

# 2. Build benchmarks
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 3. Run single test
./build/single_threaded --library=cnanolog

# 4. Run full suite (takes ~30 minutes)
./scripts/run_all.sh

# 5. Generate report
python3 scripts/compare.py
```

## Libraries Tested

| Library | Version | Description |
|---------|---------|-------------|
| **CNanoLog** | 1.0.0 | Our library - binary format, lock-free, C |
| **NanoLog** | Latest | Ultra-low latency, binary format, C++ |
| **spdlog** | 1.x | Fast, header-only, C++ |
| **Boost.Log** | 1.x | Feature-rich, Boost ecosystem |
| **glog** | Latest | Google's production logger |

## Test Scenarios

### Single-Threaded (ST)
- **ST-1:** Baseline latency measurement
- **ST-2:** Maximum throughput
- **ST-3:** Burst performance
- **ST-4:** Variable message sizes

### Multi-Threaded (MT)
- **MT-1:** Low contention (4 threads)
- **MT-2:** High contention (16 threads)
- **MT-3:** Mixed producers/consumers
- **MT-4:** Thread scaling (1-32 threads)

### Stress Tests (S)
- **S-1:** Memory pressure
- **S-2:** Disk I/O bottleneck
- **S-3:** Long duration (1 hour)
- **S-4:** Recovery from overload

### Production Simulations (P)
- **P-1:** Web server pattern
- **P-2:** Trading system (ultra-low latency)
- **P-3:** Data pipeline (high volume)

## Results

Results are stored in `results/`:
- `raw/` - JSON data files
- `reports/` - Markdown summaries
- `graphs/` - PNG visualizations

Example output:
```
┌─────────────────────────────────────────────────────────────┐
│ ST-1: Baseline Latency (1M logs, single-threaded)          │
├─────────────────────────────────────────────────────────────┤
│ Library    │ p50 (ns) │ p99 (ns) │ Throughput (M/s) │ Drops │
├────────────┼──────────┼──────────┼──────────────────┼───────┤
│ CNanoLog   │    19.8  │    50.4  │         29.4     │ 0.00% │
│ NanoLog    │     7.0  │    15.2  │         80.5     │ 0.00% │
│ spdlog     │   120.5  │   450.3  │          8.3     │ 0.00% │
│ Boost.Log  │   850.2  │  2400.8  │          1.2     │ 0.00% │
│ glog       │   310.4  │   980.6  │          3.2     │ 0.05% │
└────────────┴──────────┴──────────┴──────────────────┴───────┘
```

## System Requirements

**Minimum:**
- Ubuntu 20.04+ or similar Linux
- GCC 11+ or Clang 12+
- 8+ CPU cores
- 16GB RAM
- SSD storage

**Recommended:**
- Ubuntu 22.04
- GCC 13+
- 16+ CPU cores (for CPU isolation)
- 64GB RAM
- NVMe SSD

## Configuration

Edit `configs/*.conf` to customize:
- Buffer sizes
- Thread counts
- CPU affinity
- Flush policies

## Methodology

See [TESTING_PLAN.md](TESTING_PLAN.md) for detailed methodology, including:
- Measurement techniques
- Fair comparison rules
- Statistical validation
- Known limitations

## Contributing

To add a new library:
1. Implement `benchmark_adapter_t` interface
2. Add to `libraries/` directory
3. Update `CMakeLists.txt`
4. Run validation suite

## License

Same as CNanoLog (see root LICENSE file)
