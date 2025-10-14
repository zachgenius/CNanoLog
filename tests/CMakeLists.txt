# Tests CMakeLists.txt

# All tests link against the main cnanolog library
# Tests are built only if BUILD_TESTS is ON

# List of all test executables
set(TESTS
    simple_test
    test_format
    test_binary_writer
    test_compression
    test_e2e
    test_simple
    test_registry
    test_binary_integration
    test_statistics
    test_cacheline_alignment
    test_multithreaded_stress
    test_buffer_overflow
    test_arg_types
    benchmark_latency
    debug_count
)

# Build each test
foreach(TEST_NAME ${TESTS})
    add_executable(${TEST_NAME} ${TEST_NAME}.c)

    # Link against cnanolog library
    target_link_libraries(${TEST_NAME} cnanolog)

    # Include directories
    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )

    # Platform-specific threading libraries
    if(UNIX AND NOT APPLE)
        target_link_libraries(${TEST_NAME} pthread)
    endif()

    if(APPLE)
        target_link_libraries(${TEST_NAME} "-pthread")
    endif()

    if(WIN32)
        # Windows threading is built-in
    endif()

    # Add to CTest (except benchmarks and debug tools)
    if(NOT ${TEST_NAME} MATCHES "benchmark_" AND NOT ${TEST_NAME} MATCHES "debug_")
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    endif()
endforeach()

# Print message about tests
message(STATUS "Building tests: ${TESTS}")
