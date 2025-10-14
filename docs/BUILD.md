# Building CNanoLog Examples

## CMake Build System

CNanoLog now uses a modular CMake build system with subdirectories for better organization.

### Structure

```
CNanoLog/
├── CMakeLists.txt          # Main build configuration
├── examples/
│   ├── CMakeLists.txt      # Examples build configuration
│   ├── *.c                 # Example source files
│   └── README.md           # Examples documentation
└── tests/
    ├── CMakeLists.txt      # Tests build configuration
    └── *.c                 # Test source files
```

### Build Configuration

#### Main CMakeLists.txt
- Builds the main `cnanolog` library with all source files
- Uses `add_subdirectory(examples)` if `BUILD_EXAMPLES=ON`
- Uses `add_subdirectory(tests)` if `BUILD_TESTS=ON`

#### examples/CMakeLists.txt
- Builds 6 comprehensive example programs:
  - `basic_usage` - Getting started
  - `multithreaded` - Thread-safe logging
  - `high_performance` - CPU affinity & optimization
  - `statistics_monitoring` - Real-time monitoring
  - `production_server` - Production-ready setup
  - `error_handling` - Robustness & edge cases

#### tests/CMakeLists.txt
- Builds 15 test programs
- Automatically adds tests to CTest (except benchmarks)
- Links all tests against the main library

## Building

### Build Everything

```bash
cd CNanoLog
mkdir build && cd build
cmake ..
make -j4
```

### Build Options

```bash
# Build without examples
cmake -DBUILD_EXAMPLES=OFF ..
make

# Build without tests
cmake -DBUILD_TESTS=OFF ..
make

# Build only library
cmake -DBUILD_EXAMPLES=OFF -DBUILD_TESTS=OFF ..
make
```

### Build Specific Targets

```bash
# Build only the library
make cnanolog

# Build specific example
make basic_usage
make multithreaded
make high_performance

# Build specific test
make test_format
make test_compression
```

## Running Examples

### From build directory

```bash
cd build/examples

# Run examples
./basic_usage
./multithreaded
./high_performance
./statistics_monitoring
./production_server
./error_handling
```

### View logs

```bash
# Decompress binary logs
../tools/decompressor basic_example.clog
../tools/decompressor multithreaded_example.clog | less
../tools/decompressor server.clog | grep ERROR
```

## Running Tests

### Run all tests with CTest

```bash
cd build
ctest

# Verbose output
ctest -V

# Run specific test
ctest -R test_format
```

### Run tests manually

```bash
cd build/tests

./test_format
./test_compression
./test_e2e
./benchmark_latency
```

## Installation

### System-wide installation

```bash
cd build
sudo make install

# Now you can use CNanoLog system-wide
gcc myapp.c -lcnanolog -o myapp
```

### Custom installation prefix

```bash
cmake -DCMAKE_INSTALL_PREFIX=$HOME/local ..
make
make install

# Use with custom prefix
gcc myapp.c -I$HOME/local/include -L$HOME/local/lib -lcnanolog -o myapp
```

## Build Output

After building, you'll have:

```
build/
├── libcnanolog.a           # Static library
├── examples/
│   ├── basic_usage
│   ├── multithreaded
│   ├── high_performance
│   ├── statistics_monitoring
│   ├── production_server
│   └── error_handling
└── tests/
    ├── test_format
    ├── test_compression
    ├── test_e2e
    ├── benchmark_latency
    └── ... (11 more tests)
```

## Compiler Requirements

- **C11** standard compiler
- GCC 4.8+, Clang 3.1+, or MSVC 2015+
- pthreads (POSIX systems) or Windows threads

## Platform-Specific Notes

### Linux
- Uses pthread directly
- Full CPU affinity support
- All features supported

### macOS
- Uses pthread with "-pthread" flag
- Best-effort CPU affinity (still works!)
- All features supported

### Windows
- Uses Windows threading APIs
- Full CPU affinity support
- Requires CMake 3.10+ and MSVC 2015+

## Troubleshooting

### Error: Library not found

```bash
# Make sure library is built
make cnanolog

# Check it exists
ls libcnanolog.a
```

### Error: Examples fail to link

```bash
# Rebuild everything from scratch
cd build
rm -rf *
cmake ..
make -j4
```

### Warning: Variable length array folded to constant

This is a harmless GNU extension warning. The code uses `const int num_threads = 4` which is treated as a compile-time constant by GCC/Clang.

Can be ignored or suppressed with `-Wno-gnu-folding-constant`.

## Integration with Your Project

### Option 1: As a subdirectory

```cmake
# Your CMakeLists.txt
add_subdirectory(CNanoLog)

add_executable(myapp main.c)
target_link_libraries(myapp cnanolog)
```

### Option 2: As an installed library

```cmake
# Your CMakeLists.txt
find_library(CNANOLOG_LIB cnanolog)
find_path(CNANOLOG_INCLUDE cnanolog.h)

add_executable(myapp main.c)
target_include_directories(myapp PRIVATE ${CNANOLOG_INCLUDE})
target_link_libraries(myapp ${CNANOLOG_LIB})
```

### Option 3: Direct compilation

```bash
gcc myapp.c \
    CNanoLog/src/cnanolog_binary.c \
    CNanoLog/src/binary_writer.c \
    CNanoLog/src/compressor.c \
    CNanoLog/src/log_registry.c \
    CNanoLog/src/packer.c \
    CNanoLog/src/platform.c \
    CNanoLog/src/staging_buffer.c \
    -ICNanoLog/include \
    -lpthread \
    -o myapp
```

## Summary

✅ **Modular build system** with subdirectories
✅ **6 comprehensive examples** covering all features
✅ **15 tests** with CTest integration
✅ **Flexible build options** (examples/tests on/off)
✅ **Cross-platform support** (Linux/macOS/Windows)
✅ **Easy integration** into existing projects

All examples build successfully and demonstrate the full feature set of CNanoLog!
