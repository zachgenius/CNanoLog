cmake_minimum_required(VERSION 3.10)
project(CNanoLog C)

set(CMAKE_C_STANDARD 11)

# Library
add_library(cnanolog src/cnanolog.c src/platform.c src/ring_buffer.c)
target_include_directories(cnanolog PUBLIC include)

# Add platform-specific dependencies
if(UNIX AND NOT APPLE)
    target_link_libraries(cnanolog pthread)
endif()

if(APPLE)
    target_link_libraries(cnanolog "-pthread")
endif()


# Examples
option(BUILD_EXAMPLES "Build examples" ON)
if(BUILD_EXAMPLES)
    add_executable(simple_example examples/simple.c)
    target_link_libraries(simple_example cnanolog)
endif()

# Tests
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_executable(simple_test tests/simple_test.c)
    target_link_libraries(simple_test cnanolog)
    add_test(NAME simple_test COMMAND simple_test)
endif()