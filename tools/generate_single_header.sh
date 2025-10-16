#!/bin/bash
# generate_single_header.sh
# Generates a single-header version of CNanoLog for easy integration

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTPUT_FILE="$PROJECT_ROOT/cnanolog.h"

echo "Generating single-header file: $OUTPUT_FILE"

# Read version
VERSION=$(cat "$PROJECT_ROOT/VERSION" | tr -d '[:space:]')

cat > "$OUTPUT_FILE" << HEADER_START
/*
 * CNanoLog - Ultra-fast, low-latency binary logging library for C
 * Version: $VERSION
 *
 * This is a single-header version for easy integration.
 *
 * Usage:
 *   // In ONE .c file, define the implementation before including:
 *   #define CNANOLOG_IMPLEMENTATION
 *   #include "cnanolog.h"
 *
 *   // In all other files, just include normally:
 *   #include "cnanolog.h"
 *
 * License: MIT
 * Repository: https://github.com/zachgenius/CNanoLog
 */
HEADER_START
echo "" >> "$OUTPUT_FILE"

# Add header guard
cat >> "$OUTPUT_FILE" << 'HEADER_GUARD'
#ifndef CNANOLOG_H
#define CNANOLOG_H

/* Standard library includes */
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

HEADER_GUARD

echo "/* ============================================================================" >> "$OUTPUT_FILE"
echo " * HEADER SECTION" >> "$OUTPUT_FILE"
echo " * ============================================================================ */" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Function to strip includes and pragma once, but keep system includes in headers
strip_includes_header() {
    sed -e '/^#include "/d' \
        -e '/^#pragma once/d' \
        -e '/^\/\* Copyright/,/\*\//d' "$1"
}

# Function to strip ALL includes for implementation files
strip_includes_impl() {
    sed -e '/^#include/d' \
        -e '/^#pragma once/d' \
        -e '/^\/\* Copyright/,/\*\//d' \
        -e '/^\/\/ Copyright/,/^$/d' "$1"
}

# Add all public headers
echo "Adding public headers..."

echo "/* Format types */" >> "$OUTPUT_FILE"
strip_includes_header "$PROJECT_ROOT/include/cnanolog_format.h" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

echo "/* Type detection macros */" >> "$OUTPUT_FILE"
strip_includes_header "$PROJECT_ROOT/include/cnanolog_types.h" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

echo "/* Main API */" >> "$OUTPUT_FILE"
strip_includes_header "$PROJECT_ROOT/include/cnanolog.h" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Add private headers (needed for implementation)
echo "/* Platform abstraction */" >> "$OUTPUT_FILE"
strip_includes_header "$PROJECT_ROOT/src/platform.h" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

echo "/* Internal headers */" >> "$OUTPUT_FILE"
# Note: log_registry must come first because it defines log_site_t used by others
for header in log_registry cycles arg_packing packer compressor binary_writer staging_buffer; do
    if [ -f "$PROJECT_ROOT/src/${header}.h" ]; then
        echo "/* ${header}.h */" >> "$OUTPUT_FILE"
        strip_includes_header "$PROJECT_ROOT/src/${header}.h" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
    fi
done

# Start implementation section
cat >> "$OUTPUT_FILE" << 'IMPL_START'

/* ============================================================================
 * IMPLEMENTATION SECTION
 * ============================================================================ */

#ifdef CNANOLOG_IMPLEMENTATION

IMPL_START

echo "Adding implementation files..."

# Add all implementation files
for impl in platform compressor packer binary_writer log_registry staging_buffer cnanolog; do
    if [ -f "$PROJECT_ROOT/src/${impl}.c" ]; then
        echo "" >> "$OUTPUT_FILE"
        echo "/* ============================================================================" >> "$OUTPUT_FILE"
        echo " * ${impl}.c" >> "$OUTPUT_FILE"
        echo " * ============================================================================ */" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
        strip_includes_impl "$PROJECT_ROOT/src/${impl}.c" >> "$OUTPUT_FILE"
    fi
done

# Close implementation section
cat >> "$OUTPUT_FILE" << 'IMPL_END'

#endif /* CNANOLOG_IMPLEMENTATION */

IMPL_END

# Close header guard
cat >> "$OUTPUT_FILE" << 'FOOTER'

#ifdef __cplusplus
}
#endif

#endif /* CNANOLOG_H */
FOOTER

echo "Single-header file generated successfully: $OUTPUT_FILE"
echo "Size: $(wc -c < "$OUTPUT_FILE") bytes"
echo "Lines: $(wc -l < "$OUTPUT_FILE") lines"
