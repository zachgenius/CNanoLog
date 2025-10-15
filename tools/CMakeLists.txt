# Tools CMakeLists.txt
# Builds the decompressor utility

# Decompressor executable
add_executable(decompressor
    decompressor.c
    ${PROJECT_SOURCE_DIR}/src/packer.c
)

# Include directories
target_include_directories(decompressor PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

# Install decompressor
install(TARGETS decompressor
    RUNTIME DESTINATION bin
)

# Print message
message(STATUS "Building decompressor tool")
