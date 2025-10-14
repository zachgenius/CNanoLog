#!/bin/bash
#
# CNanoLog Benchmark Runner Script
#
# Usage:
#   ./scripts/run_benchmarks.sh [options]
#
# Options:
#   quick       - Run quick benchmarks (small to large scales)
#   full        - Run full benchmarks (including XLarge and Huge)
#   extreme     - Run extreme scale benchmark (10GB+, takes hours)
#   multithreaded - Include multi-threaded tests
#   specific <scale> - Run specific scale only
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default options
MODE="quick"
MT_FLAG=""
SCALE_FLAG=""
THREADS=4

# Check if benchmark is built
if [ ! -f "build/tests/benchmark_comprehensive" ]; then
    echo -e "${RED}Error: benchmark_comprehensive not found${NC}"
    echo "Please build first:"
    echo "  cmake -B build && cmake --build build"
    exit 1
fi

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        quick)
            MODE="quick"
            shift
            ;;
        full)
            MODE="full"
            shift
            ;;
        extreme)
            MODE="extreme"
            shift
            ;;
        multithreaded)
            MT_FLAG="--multithreaded"
            shift
            ;;
        specific)
            SCALE_FLAG="--scale $2"
            shift 2
            ;;
        --threads)
            THREADS="$2"
            shift 2
            ;;
        --help)
            echo "CNanoLog Benchmark Runner"
            echo ""
            echo "Usage: $0 [options]"
            echo ""
            echo "Modes:"
            echo "  quick         - Small to Large scales (default)"
            echo "  full          - Small to Huge scales"
            echo "  extreme       - All scales including Extreme (10GB+)"
            echo ""
            echo "Options:"
            echo "  multithreaded - Include multi-threaded tests"
            echo "  specific <scale> - Run specific scale only"
            echo "                     Scales: Tiny, Small, Medium, Large, XLarge, Huge, Extreme"
            echo "  --threads <N> - Number of threads for MT tests (default: 4)"
            echo "  --help        - Show this help"
            echo ""
            echo "Examples:"
            echo "  $0 quick                    # Quick benchmark"
            echo "  $0 full multithreaded       # Full with MT tests"
            echo "  $0 specific Medium          # Just medium scale"
            echo "  $0 extreme --threads 8      # Extreme with 8 threads"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Build command
CMD="./build/tests/benchmark_comprehensive"

case $MODE in
    quick)
        echo -e "${GREEN}Running QUICK benchmarks${NC}"
        echo -e "${BLUE}Scales: Tiny, Small, Medium, Large${NC}"
        ;;
    full)
        echo -e "${GREEN}Running FULL benchmarks${NC}"
        echo -e "${BLUE}Scales: Tiny, Small, Medium, Large, XLarge, Huge${NC}"
        echo -e "${YELLOW}This may take several minutes...${NC}"
        ;;
    extreme)
        echo -e "${GREEN}Running EXTREME benchmarks${NC}"
        echo -e "${BLUE}Scales: All including Extreme (10GB+)${NC}"
        echo -e "${RED}WARNING: This will take a LONG time (potentially hours)!${NC}"
        echo -e "${YELLOW}Press Ctrl+C within 5 seconds to cancel...${NC}"
        sleep 5
        CMD="$CMD --extreme"
        ;;
esac

if [ -n "$MT_FLAG" ]; then
    echo -e "${BLUE}Multi-threaded tests: ENABLED (${THREADS} threads)${NC}"
    CMD="$CMD $MT_FLAG --threads $THREADS"
fi

if [ -n "$SCALE_FLAG" ]; then
    CMD="$CMD $SCALE_FLAG"
fi

echo ""
echo -e "${GREEN}Starting benchmark...${NC}"
echo ""

# Create results directory
mkdir -p benchmark_results
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULT_FILE="benchmark_results/results_${TIMESTAMP}.txt"

# Run benchmark and save results
$CMD 2>&1 | tee "$RESULT_FILE"

echo ""
echo -e "${GREEN}Benchmark complete!${NC}"
echo -e "Results saved to: ${BLUE}$RESULT_FILE${NC}"
echo ""

# Summary
if [ -f "$RESULT_FILE" ]; then
    echo -e "${GREEN}═══════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}                   SUMMARY                              ${NC}"
    echo -e "${GREEN}═══════════════════════════════════════════════════════${NC}"
    grep -E "(Scale:|Throughput:|Latency|p50|Drop)" "$RESULT_FILE" | head -20
fi
