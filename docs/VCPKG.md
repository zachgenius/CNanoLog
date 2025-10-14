# vcpkg Integration Guide

This document explains how to use CNanoLog with vcpkg, Microsoft's C/C++ package manager.

## Table of Contents

- [For Users: Using CNanoLog via vcpkg](#for-users-using-cnanolog-via-vcpkg)
- [For Developers: Building with vcpkg](#for-developers-building-with-vcpkg)
- [For Maintainers: Publishing to vcpkg](#for-maintainers-publishing-to-vcpkg)

---

## For Users: Using CNanoLog via vcpkg

### Installation

Once CNanoLog is published to the vcpkg registry, install it with:

```bash
vcpkg install cnanolog
```

### Using in Your Project

#### Option 1: CMake with vcpkg toolchain

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(MyApp)

find_package(CNanoLog CONFIG REQUIRED)

add_executable(myapp main.c)
target_link_libraries(myapp PRIVATE CNanoLog::cnanolog)
```

Build with vcpkg toolchain:

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

#### Option 2: Manifest mode (vcpkg.json)

Create `vcpkg.json` in your project:

```json
{
  "name": "my-app",
  "version": "1.0.0",
  "dependencies": [
    "cnanolog"
  ]
}
```

Then build:

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

vcpkg will automatically install CNanoLog and its dependencies.

---

## For Developers: Building with vcpkg

### Local Development with vcpkg

1. **Install vcpkg** (if not already installed):

```bash
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh  # or bootstrap-vcpkg.bat on Windows
```

2. **Build CNanoLog using vcpkg toolchain**:

```bash
cd /path/to/CNanoLog
cmake -B build -S . \
    -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

3. **Install locally**:

```bash
cmake --install build --prefix /path/to/install
```

### Testing vcpkg Integration Locally

Test the package before publishing:

```bash
# Create a vcpkg overlay port
mkdir -p vcpkg-overlay/cnanolog
cp portfile.cmake vcpkg-overlay/cnanolog/
cp vcpkg.json vcpkg-overlay/cnanolog/
cp usage vcpkg-overlay/cnanolog/

# Install using overlay
vcpkg install cnanolog --overlay-ports=vcpkg-overlay
```

---

## For Maintainers: Publishing to vcpkg

### Prerequisites

1. CNanoLog must be publicly available on GitHub
2. Create a release tag (e.g., `v1.0.0`)
3. Generate SHA512 hash of the release tarball

### Step 1: Generate SHA512

```bash
# Download release tarball
wget https://github.com/zachgenius/CNanoLog/archive/refs/tags/v1.0.0.tar.gz

# Generate SHA512
sha512sum v1.0.0.tar.gz
# or on macOS:
shasum -a 512 v1.0.0.tar.gz
```

### Step 2: Update portfile.cmake

Update the `SHA512` field in `portfile.cmake` with the actual hash:

```cmake
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO zachgenius/CNanoLog
    REF v${VERSION}
    SHA512 <actual-sha512-hash>
    HEAD_REF master
)
```

### Step 3: Submit to vcpkg Registry

1. **Fork vcpkg repository**:

```bash
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
git checkout -b add-cnanolog
```

2. **Create port directory**:

```bash
mkdir -p ports/cnanolog
cp /path/to/CNanoLog/portfile.cmake ports/cnanolog/
cp /path/to/CNanoLog/vcpkg.json ports/cnanolog/
cp /path/to/CNanoLog/usage ports/cnanolog/
```

3. **Update versions database**:

```bash
./vcpkg x-add-version cnanolog
```

4. **Test the port**:

```bash
./vcpkg install cnanolog
./vcpkg remove cnanolog
```

5. **Create pull request**:

```bash
git add ports/cnanolog versions/c-/cnanolog.json
git commit -m "[cnanolog] new port"
git push origin add-cnanolog
```

Then create a PR on https://github.com/microsoft/vcpkg

### Step 4: Maintain Versioning

For each new release:

```bash
# Update vcpkg.json version
# Update portfile.cmake REF and SHA512
./vcpkg x-add-version cnanolog --overwrite-version
```

---

## vcpkg Features

### Supported Features

CNanoLog currently doesn't have optional features, but you can add them:

```json
{
  "name": "cnanolog",
  "version": "1.0.0",
  "features": {
    "tools": {
      "description": "Build decompressor tool",
      "dependencies": []
    }
  }
}
```

Users can install features with:

```bash
vcpkg install cnanolog[tools]
```

---

## Troubleshooting

### Build fails with "Cannot find pthread"

On Linux, vcpkg should automatically find pthread. If it fails:

```bash
# Install pthread development package
sudo apt-get install libpthread-stubs0-dev
```

### CMake can't find CNanoLog

Ensure you're using the vcpkg toolchain file:

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake ..
```

### Version conflicts

If you have multiple versions installed:

```bash
# Remove all versions
vcpkg remove cnanolog

# Install specific version
vcpkg install cnanolog:x64-linux
```

---

## Platform Support

CNanoLog supports:

- ✅ Linux (x64, arm64)
- ✅ macOS (x64, arm64/Apple Silicon)
- ✅ Windows (x64, x86)
- ❌ UWP (not supported)
- ❌ ARM (32-bit, not tested)

Platform-specific notes are in the main README.

---

## Resources

- **vcpkg Documentation**: https://vcpkg.io/
- **vcpkg GitHub**: https://github.com/microsoft/vcpkg
- **CNanoLog Repository**: https://github.com/zachgenius/CNanoLog
- **CMake Integration**: https://vcpkg.io/en/docs/users/buildsystems/cmake-integration.html

---

## Quick Reference

### Common Commands

```bash
# Install package
vcpkg install cnanolog

# Update package
vcpkg upgrade cnanolog

# Remove package
vcpkg remove cnanolog

# List installed packages
vcpkg list

# Search for package
vcpkg search cnanolog

# Show package info
vcpkg info cnanolog
```

### CMake Integration

```cmake
# Find package
find_package(CNanoLog CONFIG REQUIRED)

# Link to target
target_link_libraries(myapp PRIVATE CNanoLog::cnanolog)

# Get include directories (usually not needed)
target_include_directories(myapp PRIVATE ${CNANOLOG_INCLUDE_DIRS})
```

---

**For questions or issues, please file an issue on GitHub.**
