cmake_minimum_required(VERSION 3.10)

project(CNanoLogBenchmark LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Build with optimizations for accurate benchmarks
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Optimization flags
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        add_compile_options(
            -O3
            -march=native
            -mtune=native
            -flto
            -ffast-math
            -DNDEBUG
        )
    endif()
endif()

message(STATUS "Benchmark build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER_ID}")
message(STATUS "CXX Compiler: ${CMAKE_CXX_COMPILER_ID}")

# ============================================================================
# CNanoLog Library (from parent directory)
# ============================================================================

# Add parent directory's include path
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src)

# Link to CNanoLog library built in parent
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../build)

# Or build CNanoLog as part of this project
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/cnanolog)

# ============================================================================
# Benchmark Common Utilities
# ============================================================================

add_library(bench_common INTERFACE)
target_include_directories(bench_common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/common
)

# ============================================================================
# Library Adapters
# ============================================================================

# CNanoLog adapter
add_library(cnanolog_adapter STATIC
    libraries/cnanolog_adapter.c
)
target_link_libraries(cnanolog_adapter
    PUBLIC cnanolog
    PUBLIC bench_common
)

# TODO: Add other adapters (nanolog, spdlog, boost, glog)
# For now, only CNanoLog is implemented

# ============================================================================
# Benchmark Scenarios
# ============================================================================

# Single-threaded scenarios
add_executable(single_threaded
    scenarios/single_threaded.c
)
target_link_libraries(single_threaded
    PRIVATE cnanolog_adapter
    PRIVATE bench_common
)

# Large-scale scenarios (100M+ logs)
add_executable(large_scale
    scenarios/large_scale.c
)
target_link_libraries(large_scale
    PRIVATE cnanolog_adapter
    PRIVATE bench_common
)

# Platform-specific libraries
if(UNIX AND NOT APPLE)
    target_link_libraries(single_threaded PRIVATE pthread m)
    target_link_libraries(large_scale PRIVATE pthread m)
endif()

if(APPLE)
    target_link_libraries(single_threaded PRIVATE "-pthread")
    target_link_libraries(single_threaded PRIVATE m)
    target_link_libraries(large_scale PRIVATE "-pthread")
    target_link_libraries(large_scale PRIVATE m)
endif()

# TODO: Add multi-threaded, stress tests, production simulations

# ============================================================================
# Installation
# ============================================================================

install(TARGETS single_threaded large_scale
    RUNTIME DESTINATION bin
)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "CNanoLog Benchmark Suite Configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Benchmarks: single_threaded, large_scale")
message(STATUS "  Adapters:   CNanoLog")
message(STATUS "")
message(STATUS "To build:")
message(STATUS "  cmake -B build && cmake --build build")
message(STATUS "")
message(STATUS "To run:")
message(STATUS "  # Data type tests:")
message(STATUS "  ./build/single_threaded --library=cnanolog --scenario=ST-4")
message(STATUS "")
message(STATUS "  # Large-scale tests (100M+ logs):")
message(STATUS "  ./build/large_scale --library=cnanolog --scale=100M")
message(STATUS "")
