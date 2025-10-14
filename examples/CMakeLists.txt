# Examples CMakeLists.txt

# All examples link against the main cnanolog library
# Examples are built only if BUILD_EXAMPLES is ON

# List of all examples
set(EXAMPLES
    basic_usage
    multithreaded
    high_performance
    statistics_monitoring
    production_server
    error_handling
    realtime_logging
)

# Build each example
foreach(EXAMPLE ${EXAMPLES})
    add_executable(${EXAMPLE} ${EXAMPLE}.c)

    # Link against cnanolog library
    target_link_libraries(${EXAMPLE} cnanolog)

    # Include directories
    target_include_directories(${EXAMPLE} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )

    # Platform-specific threading libraries
    if(UNIX AND NOT APPLE)
        target_link_libraries(${EXAMPLE} pthread)
    endif()

    if(APPLE)
        target_link_libraries(${EXAMPLE} "-pthread")
    endif()

    if(WIN32)
        # Windows threading is built-in
    endif()
endforeach()

# Print message about examples
message(STATUS "Building ${CMAKE_CURRENT_LIST_DIR}: ${EXAMPLES}")
