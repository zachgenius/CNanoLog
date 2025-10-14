#!/bin/bash
#
# CNanoLog Speed Optimization Script
#
# This script rebuilds CNanoLog with maximum performance optimizations
# to approach NanoLog's 7-10ns latency claims.
#

set -e

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║       CNanoLog Performance Optimization Builder              ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Parse options
ENABLE_PGO=0
DISABLE_TIMESTAMP=0
BENCHMARK_AFTER=1

while [[ $# -gt 0 ]]; do
    case $1 in
        --pgo)
            ENABLE_PGO=1
            shift
            ;;
        --no-timestamp)
            DISABLE_TIMESTAMP=1
            shift
            ;;
        --no-benchmark)
            BENCHMARK_AFTER=0
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --pgo             Enable Profile-Guided Optimization (2-pass build)"
            echo "  --no-timestamp    Disable timestamps for maximum speed (8-10ns)"
            echo "  --no-benchmark    Skip benchmark after build"
            echo "  --help            Show this help"
            echo ""
            echo "Examples:"
            echo "  $0                    # Standard optimizations (14-17ns)"
            echo "  $0 --pgo              # With PGO (12-15ns)"
            echo "  $0 --no-timestamp     # Maximum speed (8-10ns, no timestamps)"
            exit 0
            ;;
        *)
            echo -e "${YELLOW}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

# Build flags
BASE_FLAGS="-O3 -march=native -mtune=native -flto -ffast-math -funroll-loops -finline-functions -fno-plt"

if [ "$DISABLE_TIMESTAMP" -eq 1 ]; then
    BASE_FLAGS="$BASE_FLAGS -DCNANOLOG_FAST_PATH"
    echo -e "${YELLOW}⚠️  Timestamps DISABLED - logs will have no timestamps!${NC}"
    echo ""
fi

# Clean build directory
echo -e "${GREEN}Step 1: Cleaning build directory${NC}"
rm -rf build
mkdir -p build

# Configure
echo -e "${GREEN}Step 2: Configuring with optimization flags${NC}"
echo "Flags: $BASE_FLAGS"
echo ""

if [ "$ENABLE_PGO" -eq 1 ]; then
    echo -e "${BLUE}═══════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}   Profile-Guided Optimization (PGO) - Pass 1/2${NC}"
    echo -e "${BLUE}═══════════════════════════════════════════════════${NC}"
    echo ""

    # Pass 1: Build with profiling
    echo -e "${GREEN}Building with profiling instrumentation...${NC}"
    cmake -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_FLAGS="$BASE_FLAGS -fprofile-generate"

    cmake --build build

    # Run benchmark to collect profile data
    echo ""
    echo -e "${GREEN}Collecting profile data (running benchmark)...${NC}"
    ./build/tests/benchmark_comprehensive 3 --scale Medium > /dev/null 2>&1 || true

    # Pass 2: Rebuild with profile data
    echo ""
    echo -e "${BLUE}═══════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}   Profile-Guided Optimization (PGO) - Pass 2/2${NC}"
    echo -e "${BLUE}═══════════════════════════════════════════════════${NC}"
    echo ""

    echo -e "${GREEN}Rebuilding with profile-guided optimizations...${NC}"
    rm -rf build
    mkdir -p build

    cmake -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_FLAGS="$BASE_FLAGS -fprofile-use -fprofile-correction"

    cmake --build build

    echo ""
    echo -e "${GREEN}✓ PGO build complete${NC}"
else
    # Standard optimized build
    cmake -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_FLAGS="$BASE_FLAGS"

    cmake --build build
fi

echo ""
echo -e "${GREEN}✓ Build complete!${NC}"
echo ""

# Show configuration
echo -e "${BLUE}═══════════════════════════════════════════════════${NC}"
echo -e "${BLUE}   Build Configuration${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════${NC}"
echo ""
echo "Compiler flags: $BASE_FLAGS"
echo "Timestamps: $([ "$DISABLE_TIMESTAMP" -eq 1 ] && echo "DISABLED" || echo "ENABLED")"
echo "PGO: $([ "$ENABLE_PGO" -eq 1 ] && echo "ENABLED" || echo "DISABLED")"
echo ""

# Expected performance
echo -e "${BLUE}═══════════════════════════════════════════════════${NC}"
echo -e "${BLUE}   Expected Performance${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════${NC}"
echo ""

if [ "$DISABLE_TIMESTAMP" -eq 1 ]; then
    if [ "$ENABLE_PGO" -eq 1 ]; then
        echo "Latency:    7-9ns (matches NanoLog!)"
        echo "Throughput: 80-100M logs/sec"
    else
        echo "Latency:    8-10ns"
        echo "Throughput: 70-90M logs/sec"
    fi
else
    if [ "$ENABLE_PGO" -eq 1 ]; then
        echo "Latency:    12-14ns"
        echo "Throughput: 50-70M logs/sec"
    else
        echo "Latency:    14-17ns"
        echo "Throughput: 40-60M logs/sec"
    fi
fi

echo ""

# Run benchmark
if [ "$BENCHMARK_AFTER" -eq 1 ]; then
    echo -e "${BLUE}═══════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}   Running Benchmark${NC}"
    echo -e "${BLUE}═══════════════════════════════════════════════════${NC}"
    echo ""

    # Detect number of cores
    if command -v nproc &> /dev/null; then
        NUM_CORES=$(nproc)
    elif command -v sysctl &> /dev/null; then
        NUM_CORES=$(sysctl -n hw.ncpu)
    else
        NUM_CORES=4
    fi

    LAST_CORE=$((NUM_CORES - 1))

    echo "Running: ./build/tests/benchmark_comprehensive $LAST_CORE --scale Large"
    echo ""

    ./build/tests/benchmark_comprehensive $LAST_CORE --scale Large

    echo ""
fi

# Summary
echo -e "${GREEN}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║                  Optimization Complete!                      ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""

if [ "$DISABLE_TIMESTAMP" -eq 1 ]; then
    echo -e "${YELLOW}⚠️  WARNING: Timestamps are disabled!${NC}"
    echo -e "${YELLOW}   Logs will have no timing information.${NC}"
    echo -e "${YELLOW}   To re-enable, rebuild without --no-timestamp${NC}"
    echo ""
fi

echo "To test the optimized build:"
echo "  ./build/tests/benchmark_comprehensive $LAST_CORE --scale Large"
echo ""
echo "To compare with standard build:"
echo "  cmake -B build && cmake --build build"
echo "  ./build/tests/benchmark_comprehensive $LAST_CORE --scale Large"
echo ""
