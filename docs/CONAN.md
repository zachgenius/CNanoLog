# Conan Integration Guide

This document explains how to use CNanoLog with Conan, the C/C++ package manager.

## Table of Contents

- [For Users: Using CNanoLog via Conan](#for-users-using-cnanolog-via-conan)
- [For Developers: Building with Conan](#for-developers-building-with-conan)
- [For Maintainers: Publishing to Conan Center](#for-maintainers-publishing-to-conan-center)

---

## For Users: Using CNanoLog via Conan

### Installation

Once CNanoLog is published to Conan Center, install it with:

```bash
conan install --requires=cnanolog/1.0.0
```

Or add to your `conanfile.txt`:

```ini
[requires]
cnanolog/1.0.0

[generators]
CMakeDeps
CMakeToolchain
```

### Using in Your Project

#### Option 1: conanfile.txt with CMake

**conanfile.txt:**
```ini
[requires]
cnanolog/1.0.0

[generators]
CMakeDeps
CMakeToolchain

[options]
cnanolog:shared=False
cnanolog:with_tools=True
```

**CMakeLists.txt:**

```cmake
cmake_minimum_required(../VERSION 3.10)
project(MyApp C)

find_package(CNanoLog REQUIRED CONFIG)

add_executable(myapp main.c)
target_link_libraries(myapp PRIVATE CNanoLog::cnanolog)
```

**Build:**
```bash
# Install dependencies
conan install . --output-folder=build --build=missing

# Configure and build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

#### Option 2: conanfile.py (Recommended)

**conanfile.py:**
```python
from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout


class MyAppConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires("cnanolog/1.0.0")

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
```

**Build:**
```bash
conan create . --build=missing
```

---

## Package Options

CNanoLog provides the following options:

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `shared` | [True, False] | False | Build as shared library |
| `fPIC` | [True, False] | True | Position Independent Code (Linux/macOS) |
| `with_tools` | [True, False] | True | Include decompressor tool |

### Examples:

```bash
# Build as shared library
conan install --requires=cnanolog/1.0.0 -o cnanolog:shared=True

# Without tools
conan install --requires=cnanolog/1.0.0 -o cnanolog:with_tools=False

# Multiple options
conan install --requires=cnanolog/1.0.0 \
    -o cnanolog:shared=True \
    -o cnanolog:with_tools=True
```

In `conanfile.txt`:
```ini
[options]
cnanolog:shared=True
cnanolog:with_tools=True
```

In `conanfile.py`:
```python
def requirements(self):
    self.requires("cnanolog/1.0.0", options={"shared": True})
```

---

## Platform-Specific Notes

### Linux
- Automatically links pthread
- Full CPU affinity support
- All features supported

### macOS
- Links CoreFoundation framework
- Best-effort CPU affinity (still works!)
- All features supported

### Windows
- Uses Windows threading APIs
- Full CPU affinity support
- Build with MSVC 2015+ or MinGW

---

## Integration Examples

### Example 1: Simple Application

**conanfile.txt:**
```ini
[requires]
cnanolog/1.0.0

[generators]
CMakeDeps
CMakeToolchain
```

**CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.10)
project(SimpleApp C)

find_package(CNanoLog REQUIRED CONFIG)

add_executable(app main.c)
target_link_libraries(app PRIVATE CNanoLog::cnanolog)
```

**main.c:**
```c
#include <cnanolog.h>
#include <stdio.h>

int main(void) {
    if (cnanolog_init("app.clog") != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    log_info("Application started");
    log_info1("Processing value: %d", 42);

    cnanolog_shutdown();
    printf("Logs written to app.clog\n");
    return 0;
}
```

**Build:**
```bash
mkdir build && cd build
conan install .. --build=missing
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
cmake --build .
```

---

### Example 2: Multi-Threaded Server

**conanfile.py:**
```python
from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout


class ServerConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires("cnanolog/1.0.0")

    def configure(self):
        # Optimize for performance
        self.options["cnanolog"].shared = False
        self.options["cnanolog"].with_tools = True

    def layout(self):
        cmake_layout(self)
```

**server.c:**
```c
#include <cnanolog.h>
#include "../src/platform.h"
#include <stdio.h>

void* worker_thread(void* arg) {
    cnanolog_preallocate();  // Important for performance!

    int id = *(int*)arg;
    log_info1("Worker %d started", id);

    for (int i = 0; i < 10000; i++) {
        log_info2("Worker %d: request %d", id, i);
    }

    log_info1("Worker %d finished", id);
    return NULL;
}

int main(void) {
    cnanolog_init("server.clog");

    // Set CPU affinity for 3x performance
    int num_cores = 8;  // Detect with sysconf() on Linux
    cnanolog_set_writer_affinity(num_cores - 1);

    cnanolog_preallocate();

    // Create worker threads
    const int num_workers = 4;
    cnanolog_thread_t threads[num_workers];
    int ids[num_workers];

    for (int i = 0; i < num_workers; i++) {
        ids[i] = i;
        cnanolog_thread_create(&threads[i], worker_thread, &ids[i]);
    }

    // Wait for completion
    for (int i = 0; i < num_workers; i++) {
        cnanolog_thread_join(threads[i], NULL);
    }

    // Get statistics
    cnanolog_stats_t stats;
    cnanolog_get_stats(&stats);
    printf("Total logs: %llu\n", (unsigned long long)stats.total_logs_written);
    printf("Drop rate: %.2f%%\n",
           (stats.dropped_logs * 100.0) / (stats.total_logs_written + stats.dropped_logs));

    cnanolog_shutdown();
    return 0;
}
```

---

## For Developers: Building with Conan

### Local Development

1. **Install Conan** (if not already installed):

```bash
pip install conan
```

2. **Create a default profile**:

```bash
conan profile detect --force
```

3. **Build locally**:

```bash
cd CNanoLog

# Install dependencies and build
conan create . --build=missing

# Or install to Conan cache
conan export . --version=1.0.0
conan install --requires=cnanolog/1.0.0 --build=missing
```

### Testing the Package Locally

```bash
# Run Conan's built-in test
conan create . --build=missing

# The test_package will be built and run automatically
```

### Different Build Configurations

```bash
# Debug build
conan create . --build=missing -s build_type=Debug

# Release build
conan create . --build=missing -s build_type=Release

# Shared library
conan create . --build=missing -o shared=True

# Without tools
conan create . --build=missing -o with_tools=False
```

### Cross-Compilation

```bash
# For ARM64 Linux
conan create . --build=missing -s arch=armv8

# For Windows from Linux
conan create . --build=missing -s os=Windows -s compiler=gcc
```

---

## For Maintainers: Publishing to Conan Center

### Prerequisites

1. CNanoLog must be publicly available on GitHub
2. Create a release tag (e.g., `v1.0.0`)
3. Fork the [conan-center-index](https://github.com/conan-io/conan-center-index) repository

### Step 1: Create a Recipe for Conan Center

The recipe is already in your repository (`conanfile.py`), but for Conan Center you need a separate recipe in the conan-center-index repository.

### Step 2: Fork and Clone conan-center-index

```bash
# Fork on GitHub first
git clone https://github.com/YOUR_USERNAME/conan-center-index.git
cd conan-center-index
git checkout -b cnanolog-1.0.0
```

### Step 3: Create Recipe Directory

```bash
mkdir -p recipes/cnanolog/all
cd recipes/cnanolog
```

### Step 4: Create config.yml

**recipes/cnanolog/config.yml:**
```yaml
versions:
  "1.0.0":
    folder: all
```

### Step 5: Copy and Adapt Recipe

Copy your `conanfile.py` to `recipes/cnanolog/all/` and modify it:

**recipes/cnanolog/all/conanfile.py:**
```python
from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout
from conan.tools.files import get, copy, rmdir
import os

required_conan_version = ">=1.53.0"


class CNanoLogConan(ConanFile):
    name = "cnanolog"
    description = "Ultra-fast, low-latency binary logging library for C11"
    license = "MIT"
    url = "https://github.com/conan-io/conan-center-index"
    homepage = "https://github.com/zachgenius/CNanoLog"
    topics = ("logging", "performance", "binary-logging", "c11", "low-latency")

    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_tools": [True, False],
    }

    default_options = {
        "shared": False,
        "fPIC": True,
        "with_tools": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def layout(self):
        cmake_layout(self, src_folder="src")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_EXAMPLES"] = False
        tc.variables["BUILD_TESTS"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder,
             dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

        if not self.options.with_tools:
            rmdir(self, os.path.join(self.package_folder, "bin"))

        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))

    def package_info(self):
        self.cpp_info.libs = ["cnanolog"]

        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.extend(["pthread", "m"])
        elif self.settings.os == "Macos":
            self.cpp_info.frameworks.append("CoreFoundation")

        if self.options.with_tools:
            bin_path = os.path.join(self.package_folder, "bin")
            self.buildenv_info.prepend_path("PATH", bin_path)
```

### Step 6: Create conandata.yml

**recipes/cnanolog/all/conandata.yml:**
```yaml
sources:
  "1.0.0":
    url: "https://github.com/zachgenius/CNanoLog/archive/refs/tags/v1.0.0.tar.gz"
    sha256: "YOUR_SHA256_HERE"
```

Generate SHA256:
```bash
wget https://github.com/zachgenius/CNanoLog/archive/refs/tags/v1.0.0.tar.gz
sha256sum v1.0.0.tar.gz
```

### Step 7: Copy test_package

```bash
cp -r /path/to/CNanoLog/test_package recipes/cnanolog/all/
```

### Step 8: Test Locally

```bash
cd recipes/cnanolog/all
conan create . --version=1.0.0 --build=missing
```

### Step 9: Submit Pull Request

```bash
git add recipes/cnanolog
git commit -m "cnanolog/1.0.0: new recipe"
git push origin cnanolog-1.0.0
```

Then create a PR on https://github.com/conan-io/conan-center-index

---

## Troubleshooting

### Issue: Conan can't find pthread

**Solution:** This is handled automatically in the recipe. If issues persist:

```python
# In your conanfile.py
def system_requirements(self):
    if self.settings.os == "Linux":
        apt = Apt(self)
        apt.install(["libpthread-stubs0-dev"])
```

### Issue: CMake can't find CNanoLog

**Solution:** Ensure you're using the CMake toolchain from Conan:

```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
```

### Issue: Linking errors on Windows

**Solution:** Ensure you're using the same runtime library:

```bash
conan install . -s compiler.runtime=dynamic
```

---

## Conan vs vcpkg

Both package managers are supported. Choose based on your needs:

| Feature | Conan | vcpkg |
|---------|-------|-------|
| Language | Python-based | CMake-based |
| Binary packages | Yes (by default) | Yes (with binary caching) |
| Custom repositories | Easy | Requires overlay |
| Integration | Good | Excellent (CMake) |
| Versioning | Flexible | Git-based |
| Learning curve | Moderate | Easy |

**Recommendation:**
- Use **vcpkg** for CMake-centric projects on Windows
- Use **Conan** for cross-platform, complex dependency scenarios

---

## Quick Reference

### Common Commands

```bash
# Install package
conan install --requires=cnanolog/1.0.0

# Create package from source
conan create . --build=missing

# Search for package
conan search cnanolog

# Show package info
conan inspect cnanolog/1.0.0

# Remove package
conan remove cnanolog/1.0.0

# List installed packages
conan list "*"
```

### Profile Management

```bash
# Show current profile
conan profile show

# Create custom profile
conan profile detect --name myprofile

# Use specific profile
conan install . --profile=myprofile
```

---

## Resources

- **Conan Documentation**: https://docs.conan.io/
- **Conan Center**: https://conan.io/center
- **CNanoLog Repository**: https://github.com/zachgenius/CNanoLog
- **Conan Center Index**: https://github.com/conan-io/conan-center-index

---

**For questions or issues, please file an issue on GitHub.**
