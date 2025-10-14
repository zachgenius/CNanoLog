cmake_minimum_required(VERSION 3.10)
project(CNanoLog VERSION 1.0.0 LANGUAGES C)

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

# Print installation summary
message(STATUS "CNanoLog ${PROJECT_VERSION} configuration:")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Library dir:    ${CMAKE_INSTALL_LIBDIR}")
message(STATUS "  Include dir:    ${CMAKE_INSTALL_INCLUDEDIR}")
message(STATUS "  CMake dir:      ${CONFIG_INSTALL_DIR}")