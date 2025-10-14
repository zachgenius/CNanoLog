# vcpkg portfile for CNanoLog
# This file describes how vcpkg should build and install CNanoLog

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO zachgenius/CNanoLog
    REF v${VERSION}
    SHA512 0  # Update with actual SHA512 of release tarball
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_EXAMPLES=OFF
        -DBUILD_TESTS=OFF
)

vcpkg_cmake_build()

vcpkg_cmake_install()

# Remove duplicate headers
vcpkg_cmake_config_fixup(
    PACKAGE_NAME CNanoLog
    CONFIG_PATH lib/cmake/CNanoLog
)

# Install license
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

# Remove unnecessary files
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

# Copy usage file
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage"
     DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
