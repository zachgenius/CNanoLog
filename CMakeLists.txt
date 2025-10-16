cmake_minimum_required(VERSION 3.10)

# Read version from VERSION file
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" CNANOLOG_VERSION)
string(STRIP "${CNANOLOG_VERSION}" CNANOLOG_VERSION)

project(CNanoLog VERSION ${CNANOLOG_VERSION} LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

# ============================================================================
# Configuration Options
# ============================================================================

option(CNANOLOG_ENABLE_TIMESTAMPS "Enable high-resolution timestamps (rdtsc). Disable for maximum throughput." ON)

# ============================================================================
# Build Type and Optimization Flags
# ============================================================================

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING
        "Choose the type of build: Debug Release RelWithDebInfo MinSizeRel" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# Optimization flags for Release builds
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    # GCC and Clang
    set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -g -DNDEBUG")
    set(CMAKE_C_FLAGS_DEBUG "-O0 -g")

    # Additional performance flags for Release
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        add_compile_options(
            -march=native          # Optimize for current CPU
            -ffast-math           # Aggressive math optimizations
            -funroll-loops        # Unroll loops for speed
            -finline-functions    # Aggressive inlining
        )
    endif()
elseif(MSVC)
    # MSVC
    set(CMAKE_C_FLAGS_RELEASE "/O2 /Ob2 /DNDEBUG")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "/O2 /Ob2 /Zi /DNDEBUG")
    set(CMAKE_C_FLAGS_DEBUG "/Od /Zi")
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER_ID}")
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Optimization: Enabled (-O3 with performance flags)")
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Optimization: Disabled (Debug mode)")
endif()

# ============================================================================
# Library Definition
# ============================================================================

# Library - can be built as STATIC or SHARED
add_library(cnanolog
    src/cnanolog.c
    src/platform.c
    src/binary_writer.c
    src/compressor.c
    src/log_registry.c
    src/packer.c
    src/staging_buffer.c
)

# Support building as shared library
if(BUILD_SHARED_LIBS)
    set_target_properties(cnanolog PROPERTIES
        C_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
endif()

# Set library properties
set_target_properties(cnanolog PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "include/cnanolog.h;include/cnanolog_format.h"
)

# Include directories
target_include_directories(cnanolog
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Apply timestamp configuration
if(NOT CNANOLOG_ENABLE_TIMESTAMPS)
    target_compile_definitions(cnanolog PUBLIC CNANOLOG_NO_TIMESTAMPS)
    message(STATUS "Timestamps: DISABLED (extreme performance mode)")
else()
    message(STATUS "Timestamps: ENABLED (rdtsc high-resolution)")
endif()

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

# ============================================================================
# Installation Configuration for vcpkg
# ============================================================================

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install library
install(TARGETS cnanolog
    EXPORT CNanoLogTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install additional headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# Create and install CMake config files
set(CONFIG_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/CNanoLog)

# Generate version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/CNanoLogConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Configure config file
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CNanoLogConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/CNanoLogConfig.cmake"
    INSTALL_DESTINATION ${CONFIG_INSTALL_DIR}
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR
)

# Install CMake config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/CNanoLogConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/CNanoLogConfigVersion.cmake"
    DESTINATION ${CONFIG_INSTALL_DIR}
)

# Install targets file
install(EXPORT CNanoLogTargets
    FILE CNanoLogTargets.cmake
    NAMESPACE CNanoLog::
    DESTINATION ${CONFIG_INSTALL_DIR}
)

# Export targets for use in build tree
export(EXPORT CNanoLogTargets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/CNanoLogTargets.cmake"
    NAMESPACE CNanoLog::
)

# Register package in user's package registry (optional)
export(PACKAGE CNanoLog)

# ============================================================================
# Single-Header Generation Target
# ============================================================================

add_custom_target(single-header
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_single_header.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating single-header version: cnanolog.h"
    VERBATIM
)

# Print installation summary
message(STATUS "CNanoLog ${PROJECT_VERSION} configuration:")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Library dir:    ${CMAKE_INSTALL_LIBDIR}")
message(STATUS "  Include dir:    ${CMAKE_INSTALL_INCLUDEDIR}")
message(STATUS "  CMake dir:      ${CONFIG_INSTALL_DIR}")
message(STATUS "")
message(STATUS "To generate single-header file: make single-header")