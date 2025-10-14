#!/bin/bash
#
# Run All Benchmarks Script
#
# Runs the complete benchmark suite and generates reports.
#

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     CNanoLog Complete Benchmark Suite                       ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Check if benchmarks are built
if [ ! -f "build/single_threaded" ]; then
    echo -e "${YELLOW}Benchmarks not built. Building now...${NC}"
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    echo ""
fi

# Create timestamp for this run
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULT_DIR="results/raw/${TIMESTAMP}"
mkdir -p "$RESULT_DIR"

echo -e "${GREEN}Running benchmarks (results in $RESULT_DIR)${NC}"
echo ""

# ============================================================================
# Single-Threaded Benchmarks
# ============================================================================

echo -e "${BLUE}═══════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  Single-Threaded Benchmarks${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════${NC}"
echo ""

# ST-1: Baseline Latency
echo -e "${GREEN}Running ST-1: Baseline Latency...${NC}"
./build/single_threaded --library=cnanolog --scenario=ST-1 | tee "$RESULT_DIR/ST-1-cnanolog.log"
echo ""

# TODO: Add ST-2, ST-3, ST-4 when implemented

# ============================================================================
# Multi-Threaded Benchmarks (TODO)
# ============================================================================

# echo -e "${BLUE}═══════════════════════════════════════════════════════${NC}"
# echo -e "${BLUE}  Multi-Threaded Benchmarks${NC}"
# echo -e "${BLUE}═══════════════════════════════════════════════════════${NC}"
# echo ""
# MT tests not implemented yet

# ============================================================================
# Summary
# ============================================================================

echo ""
echo -e "${GREEN}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║ Benchmark Suite Complete                                     ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""

echo "Results saved to: $RESULT_DIR"
echo ""

echo "To generate comparison report:"
echo "  # TODO: Implement compare.py script"
echo "  # python3 scripts/compare.py $RESULT_DIR/*.log"
echo ""

echo "To visualize results:"
echo "  # TODO: Implement visualize.py script"
echo "  # python3 scripts/visualize.py $RESULT_DIR/*.log"
echo ""
