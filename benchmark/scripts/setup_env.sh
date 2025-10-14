#!/bin/bash
#
# Setup Script for CNanoLog Benchmark Suite
#
# Prepares the system for accurate benchmarking.
#

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     CNanoLog Benchmark Environment Setup                    ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo -e "${YELLOW}Warning: Not running as root. Some optimizations will be skipped.${NC}"
    echo -e "${YELLOW}         Run with sudo for full setup.${NC}"
    echo ""
fi

# ============================================================================
# System Information
# ============================================================================

echo -e "${GREEN}System Information:${NC}"
echo "  OS:      $(uname -s) $(uname -r)"
echo "  CPU:     $(grep 'model name' /proc/cpuinfo 2>/dev/null | head -1 | cut -d: -f2 | xargs || sysctl -n machdep.cpu.brand_string 2>/dev/null || echo 'Unknown')"
echo "  Cores:   $(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 'Unknown')"
echo "  RAM:     $(free -h 2>/dev/null | grep Mem | awk '{print $2}' || sysctl -n hw.memsize 2>/dev/null | awk '{print $1/1024/1024/1024 " GB"}' || echo 'Unknown')"
echo ""

# ============================================================================
# CPU Frequency Scaling (Linux only)
# ============================================================================

if [ "$EUID" -eq 0 ] && [ "$(uname)" = "Linux" ]; then
    echo -e "${GREEN}Setting CPU governor to 'performance'...${NC}"

    for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
        if [ -f "$cpu" ]; then
            echo performance > "$cpu"
        fi
    done

    echo "  ✓ CPU frequency locked to maximum"
else
    echo -e "${YELLOW}Skipping CPU frequency setup (need root on Linux)${NC}"
    if [ "$(uname)" = "Darwin" ]; then
        echo "  Note: macOS manages CPU frequency automatically"
    fi
fi
echo ""

# ============================================================================
# Transparent Huge Pages (Linux only)
# ============================================================================

if [ "$EUID" -eq 0 ] && [ "$(uname)" = "Linux" ]; then
    echo -e "${GREEN}Disabling transparent huge pages...${NC}"

    if [ -f /sys/kernel/mm/transparent_hugepage/enabled ]; then
        echo never > /sys/kernel/mm/transparent_hugepage/enabled
        echo "  ✓ Transparent huge pages disabled"
    else
        echo "  ⚠ File not found, skipping"
    fi
else
    echo -e "${YELLOW}Skipping transparent huge pages setup (need root on Linux)${NC}"
fi
echo ""

# ============================================================================
# File Descriptor Limits
# ============================================================================

echo -e "${GREEN}Increasing file descriptor limits...${NC}"

# Try to increase limits
ulimit -n 65536 2>/dev/null || echo "  ⚠ Could not set ulimit (current: $(ulimit -n))"

current_limit=$(ulimit -n)
echo "  File descriptors: $current_limit"
echo ""

# ============================================================================
# Dependencies Check
# ============================================================================

echo -e "${GREEN}Checking dependencies...${NC}"

# CMake
if command -v cmake &> /dev/null; then
    CMAKE_VERSION=$(cmake --version | head -1 | cut -d' ' -f3)
    echo "  ✓ CMake $CMAKE_VERSION"
else
    echo -e "  ${RED}✗ CMake not found${NC}"
    echo "    Install: sudo apt install cmake (Ubuntu) or brew install cmake (macOS)"
fi

# GCC/Clang
if command -v gcc &> /dev/null; then
    GCC_VERSION=$(gcc --version | head -1)
    echo "  ✓ $GCC_VERSION"
elif command -v clang &> /dev/null; then
    CLANG_VERSION=$(clang --version | head -1)
    echo "  ✓ $CLANG_VERSION"
else
    echo -e "  ${RED}✗ C compiler not found${NC}"
    echo "    Install: sudo apt install build-essential (Ubuntu)"
fi

# Python (for analysis scripts)
if command -v python3 &> /dev/null; then
    PYTHON_VERSION=$(python3 --version | cut -d' ' -f2)
    echo "  ✓ Python $PYTHON_VERSION"
else
    echo -e "  ${YELLOW}⚠ Python3 not found (needed for analysis scripts)${NC}"
fi

echo ""

# ============================================================================
# Create Directory Structure
# ============================================================================

echo -e "${GREEN}Creating directory structure...${NC}"

mkdir -p results/raw
mkdir -p results/reports
mkdir -p results/graphs
mkdir -p build

echo "  ✓ Created results directories"
echo ""

# ============================================================================
# Build CNanoLog
# ============================================================================

echo -e "${GREEN}Building CNanoLog library...${NC}"

cd ..
if [ ! -d "build" ]; then
    mkdir build
fi

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target cnanolog

echo "  ✓ CNanoLog built"
cd benchmark
echo ""

# ============================================================================
# Summary
# ============================================================================

echo -e "${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║ Environment Setup Complete                                   ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""

echo "Next steps:"
echo "  1. Build benchmarks:"
echo "     cmake -B build && cmake --build build"
echo ""
echo "  2. Run single test:"
echo "     ./build/single_threaded --library=cnanolog --scenario=ST-1"
echo ""
echo "  3. Run full suite:"
echo "     ./scripts/run_all.sh"
echo ""

if [ "$EUID" -ne 0 ]; then
    echo -e "${YELLOW}For best results, run with sudo for CPU frequency locking:${NC}"
    echo "  sudo ./scripts/setup_env.sh"
    echo ""
fi
