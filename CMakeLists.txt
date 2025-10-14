cmake_minimum_required(VERSION 3.10)
project(CNanoLog C)

set(CMAKE_C_STANDARD 11)

# Library
add_library(cnanolog
    src/cnanolog.c
    src/cnanolog_binary.c
    src/platform.c
    src/ring_buffer.c
    src/binary_writer.c
    src/compressor.c
    src/log_registry.c
    src/packer.c
    src/staging_buffer.c
)
target_include_directories(cnanolog PUBLIC include)

# Add platform-specific dependencies
if(UNIX AND NOT APPLE)
    target_link_libraries(cnanolog pthread)
endif()

if(APPLE)
    target_link_libraries(cnanolog "-pthread")
endif()


# Tools (decompressor)
add_subdirectory(tools)

# Examples
option(BUILD_EXAMPLES "Build examples" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()