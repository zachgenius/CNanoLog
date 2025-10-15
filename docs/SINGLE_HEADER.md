# Single-Header Usage Guide

CNanoLog provides a single-header version for easy integration into your projects, similar to popular libraries like stb_image.h.

## Quick Start

### 1. Generate the Single-Header File

```bash
cd CNanoLog
./tools/generate_single_header.sh
```

This generates `cnanolog.h` in the root directory (~120KB, 3600 lines).

Alternatively, if using CMake:
```bash
make single-header
```

### 2. Copy to Your Project

```bash
cp cnanolog.h your_project/include/
```

### 3. Use in Your Code

**In ONE .c file** (typically your main.c or a dedicated cnanolog.c):
```c
#define CNANOLOG_IMPLEMENTATION
#include "cnanolog.h"
```

**In all other files:**
```c
#include "cnanolog.h"
```

### 4. Compile

```bash
gcc -std=c11 -pthread your_app.c -o your_app
```

**Platform-specific notes:**
- **Linux**: Add `-pthread` flag
- **macOS**: Add `-pthread` flag
- **Windows**: Link with appropriate threading library

## Complete Example

```c
// main.c
#define CNANOLOG_IMPLEMENTATION
#include "cnanolog.h"

int main(void) {
    // Initialize
    cnanolog_init("app.clog");

    // Log messages
    log_info("Application started");
    log_info1("Processing %d items", 100);
    log_warn1("Memory usage: %d MB", 512);
    log_error1("Connection failed: code %d", -1);

    // Shutdown
    cnanolog_shutdown();
    return 0;
}
```

Compile:
```bash
gcc -std=c11 -pthread main.c -o app
```

## Advantages of Single-Header

✅ **Simplicity** - Just one file to copy
✅ **No build system** - No CMake/Make required
✅ **Self-contained** - No external dependencies
✅ **Zero configuration** - Works out of the box
✅ **Portable** - Easy to vendor into your project

## Technical Details

- **Size**: ~120 KB uncompressed
- **Lines**: ~3600 lines of code
- **Dependencies**: Only standard C11 library
- **Threading**: Uses pthread (POSIX) or Windows threads
- **Generated from**: All source files in `src/` and `include/`

## Regenerating

The single-header file is **generated** from source, so:

1. Don't edit `cnanolog.h` directly
2. Make changes to source files in `src/` and `include/`
3. Regenerate with `./tools/generate_single_header.sh`

## Comparison with Other Integration Methods

| Method | Setup Complexity | Build Speed | Flexibility |
|--------|-----------------|-------------|-------------|
| **Single-header** | ⭐ Very Easy | Fast | Limited |
| Compiled library | Medium | Very Fast | High |
| Source integration | Medium | Slow | Very High |
| Package managers | Easy | Very Fast | Medium |

## Troubleshooting

### Compilation errors about pthread

**Solution**: Add `-pthread` flag to your compiler command.

### Linker errors about multiple definitions

**Problem**: Multiple .c files have `#define CNANOLOG_IMPLEMENTATION`
**Solution**: Only define it in ONE .c file.

### "C11 required" error

**Solution**: Add `-std=c11` flag to your compiler command.

## See Also

- [README.md](../README.md) - Main documentation
- [examples/single_header_example.c](../examples/single_header_example.c) - Complete example
- [tools/generate_single_header.sh](../tools/generate_single_header.sh) - Generation script
