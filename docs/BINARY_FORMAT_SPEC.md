# CNanoLog Binary Format Specification v1.0

## Overview

This document defines the binary log file format for CNanoLog. The format is designed to be:
- **Compact**: Minimal overhead per log entry
- **Fast to write**: Sequential writes, no seeks during logging
- **Self-describing**: Contains all metadata needed for decompression
- **Extensible**: Version field allows future format changes
- **Platform-independent**: Explicit endianness specification

## File Structure

```
┌─────────────────────────────────────────────────┐
│ File Header (64 bytes)                          │
├─────────────────────────────────────────────────┤
│ Log Entries (variable length)                   │
│   ┌─────────────────────────────────────────┐  │
│   │ Entry 1                                  │  │
│   ├─────────────────────────────────────────┤  │
│   │ Entry 2                                  │  │
│   ├─────────────────────────────────────────┤  │
│   │ ...                                      │  │
│   └─────────────────────────────────────────┘  │
├─────────────────────────────────────────────────┤
│ Dictionary (at end of file)                     │
│   ┌─────────────────────────────────────────┐  │
│   │ Dictionary Header                        │  │
│   ├─────────────────────────────────────────┤  │
│   │ Log Site 1                               │  │
│   ├─────────────────────────────────────────┤  │
│   │ Log Site 2                               │  │
│   ├─────────────────────────────────────────┤  │
│   │ ...                                      │  │
│   └─────────────────────────────────────────┘  │
└─────────────────────────────────────────────────┘
```

---

## 1. File Header (64 bytes)

The file begins with a fixed 64-byte header.

### Structure

```c
typedef struct {
    uint32_t magic;              // Magic number: 0x4E414E4F ("NANO")
    uint16_t version_major;      // Format version major (1)
    uint16_t version_minor;      // Format version minor (0)
    uint64_t timestamp_frequency; // rdtsc() ticks per second
    uint64_t start_timestamp;    // rdtsc() value at log start
    int64_t  start_time_sec;     // Unix epoch seconds at log start
    int32_t  start_time_nsec;    // Nanoseconds component
    uint32_t endianness;         // 0x01020304 for endian detection
    uint64_t dictionary_offset;  // Byte offset to dictionary (0 = end of file)
    uint32_t entry_count;        // Total number of log entries
    uint8_t  reserved[12];       // Reserved for future use (must be 0)
} __attribute__((packed)) cnanolog_file_header_t;
```

### Field Descriptions

| Field | Size | Offset | Description |
|-------|------|--------|-------------|
| `magic` | 4 | 0 | Magic number 0x4E414E4F ("NANO" in ASCII). Used to identify file type. |
| `version_major` | 2 | 4 | Format version major. Current: 1. Breaking changes increment this. |
| `version_minor` | 2 | 6 | Format version minor. Current: 0. Compatible changes increment this. |
| `timestamp_frequency` | 8 | 8 | CPU timestamp frequency in ticks/second. Used to convert rdtsc() to time. |
| `start_timestamp` | 8 | 16 | rdtsc() value when logging started. Reference point for relative times. |
| `start_time_sec` | 8 | 24 | Unix epoch seconds when logging started (from `time()`). |
| `start_time_nsec` | 4 | 32 | Nanoseconds component of start time (0-999999999). |
| `endianness` | 4 | 36 | Always 0x01020304. Decompressor checks byte order. |
| `dictionary_offset` | 8 | 40 | Byte offset from start of file to dictionary. 0 = at end of file. |
| `entry_count` | 4 | 48 | Total log entries written. Updated at shutdown. |
| `reserved` | 12 | 52 | Reserved for future use. Must be zeroed. |

**Total: 64 bytes**

### Example (Hex Dump)

```
Offset  00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  ASCII
------  -----------------------------------------------  ----------------
0x0000  4E 41 4E 4F 01 00 00 00 00 CA 9A 3B 00 00 00 00  NANO.......;....
        \_________/ \___/ \___/ \____________________/
         "NANO"     v1.0   v1.0   ticks/sec (1B hex)
0x0010  78 56 34 12 00 00 00 00 67 3E 57 27 00 00 00 00  xV4.....g>W'....
        \_____________________/ \_____________________/
         start_timestamp         start_time_sec
0x0020  00 00 00 00 04 03 02 01 00 00 00 00 00 00 00 00  ................
        \_________/ \_________/ \_____________________/
         nsec        endian      dictionary_offset (0)
0x0030  0F 27 00 00 00 00 00 00 00 00 00 00 00 00 00 00  .'..............
        \_________/ \______________________________/
         entry_count reserved (zeros)
```

---

## 2. Log Entry Format

Each log entry has a variable-length format consisting of a header followed by argument data.

### Entry Header

```c
typedef struct {
    uint32_t log_id;           // Log site identifier
    uint64_t timestamp;        // rdtsc() value when logged
    uint16_t data_length;      // Bytes of argument data following
} __attribute__((packed)) cnanolog_entry_header_t;
```

**Size: 14 bytes**

### Complete Entry Layout

```
┌─────────────────────────────────────┐
│ Entry Header (14 bytes)             │
│  - log_id (4 bytes)                 │
│  - timestamp (8 bytes)              │
│  - data_length (2 bytes)            │
├─────────────────────────────────────┤
│ Argument Data (data_length bytes)   │
│  - Raw binary arguments             │
│  - Format depends on log site type  │
└─────────────────────────────────────┘
```

### Argument Data Format

Arguments are stored in the order they appear in the format string:

#### For Non-String Types (int, long, double, pointer)
- Stored as **full-width values** (uncompressed)
- int32_t → 4 bytes
- int64_t → 8 bytes
- double → 8 bytes
- void* → 8 bytes (64-bit systems)

#### For String Types (char*, const char*)
- **4-byte length prefix** (uint32_t) + string data (no null terminator)
- Length includes only the string bytes, not the prefix itself

#### Example Entry

```c
log_info_ii("Count: %d, Total: %d", 42, 1000);
```

**Binary Layout:**
```
Offset  Bytes                           Description
------  ------------------------------  ---------------------
0       [2A 00 00 00]                  log_id = 42
4       [78 56 34 12 00 00 00 00]      timestamp = 0x12345678
12      [08 00]                         data_length = 8 bytes
14      [2A 00 00 00]                  arg1: int = 42
18      [E8 03 00 00]                  arg2: int = 1000
```

**Total: 22 bytes**

#### Example with String

```c
log_info_si("User: %s, ID: %d", "alice", 123);
```

**Binary Layout:**
```
Offset  Bytes                           Description
------  ------------------------------  ---------------------
0       [2B 00 00 00]                  log_id = 43
4       [79 56 34 12 00 00 00 00]      timestamp
12      [0D 00]                         data_length = 13 bytes
14      [05 00 00 00]                  string length = 5
18      [61 6C 69 63 65]               "alice" (no null)
23      [7B 00 00 00]                  int = 123
```

**Total: 27 bytes**

---

## 3. Dictionary Format

The dictionary contains metadata for all log sites encountered during execution. It's written at the end of the file during `cnanolog_shutdown()`.

### Dictionary Header

```c
typedef struct {
    uint32_t magic;              // 0x44494354 ("DICT")
    uint32_t num_entries;        // Number of log site entries
    uint32_t total_size;         // Total size of dictionary in bytes
    uint32_t reserved;           // Reserved (must be 0)
} __attribute__((packed)) cnanolog_dict_header_t;
```

**Size: 16 bytes**

### Dictionary Entry Format

Each log site in the dictionary has this format:

```c
typedef struct {
    uint32_t log_id;             // Unique log identifier
    uint8_t  log_level;          // 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR
    uint8_t  num_args;           // Number of format arguments
    uint16_t filename_length;    // Length of filename string
    uint16_t format_length;      // Length of format string
    uint32_t line_number;        // Line number in source file
    uint8_t  arg_types[16];      // Array of argument type codes (max 16 args)
    // Followed by:
    //   - filename string (filename_length bytes, no null)
    //   - format string (format_length bytes, no null)
} __attribute__((packed)) cnanolog_dict_entry_t;
```

**Fixed size: 30 bytes** (+ variable string data)

### Argument Type Codes

```c
typedef enum {
    ARG_TYPE_NONE    = 0,   // No argument (placeholder)
    ARG_TYPE_INT32   = 1,   // int32_t, int
    ARG_TYPE_INT64   = 2,   // int64_t, long, long long
    ARG_TYPE_UINT32  = 3,   // uint32_t, unsigned int
    ARG_TYPE_UINT64  = 4,   // uint64_t, unsigned long
    ARG_TYPE_DOUBLE  = 5,   // double, float (promoted to double)
    ARG_TYPE_STRING  = 6,   // char*, const char*
    ARG_TYPE_POINTER = 7,   // void*, any pointer type
} cnanolog_arg_type_t;
```

### Complete Dictionary Example

```
┌───────────────────────────────────────────────┐
│ Dictionary Header (16 bytes)                  │
│  magic = 0x44494354 ("DICT")                 │
│  num_entries = 3                              │
│  total_size = 196                             │
├───────────────────────────────────────────────┤
│ Entry 1 (30 + 31 bytes = 61 bytes)           │
│  log_id = 0                                   │
│  log_level = 1 (INFO)                         │
│  num_args = 0                                 │
│  filename = "main.c" (6 bytes)                │
│  format = "Application started" (19 bytes)    │
│  line = 42                                    │
├───────────────────────────────────────────────┤
│ Entry 2 (30 + 27 bytes = 57 bytes)           │
│  log_id = 1                                   │
│  log_level = 1 (INFO)                         │
│  num_args = 1                                 │
│  arg_types = [1, ...]  (INT32)                │
│  filename = "main.c" (6 bytes)                │
│  format = "Count: %d" (10 bytes)              │
│  line = 45                                    │
├───────────────────────────────────────────────┤
│ Entry 3 (30 + 32 bytes = 62 bytes)           │
│  log_id = 2                                   │
│  log_level = 2 (WARN)                         │
│  num_args = 2                                 │
│  arg_types = [6, 1, ...]  (STRING, INT32)    │
│  filename = "user.c" (6 bytes)                │
│  format = "User %s, ID: %d" (15 bytes)        │
│  line = 128                                   │
└───────────────────────────────────────────────┘
Total: 16 + 61 + 57 + 62 = 196 bytes
```

---

## 4. Complete File Example

### Source Code
```c
cnanolog_init("app.clog");

log_info("Started");                    // log_id = 0
log_info_i("Processing %d items", 5);   // log_id = 1
log_warn_si("User %s failed", "bob");   // log_id = 2

cnanolog_shutdown();
```

### Binary File Layout

```
┌─────────────────────────────────────────────────┐
│ Offset 0: File Header (64 bytes)               │
│   magic = 0x4E414E4F                            │
│   version = 1.0                                 │
│   timestamp_frequency = 1000000000              │
│   entry_count = 3                               │
├─────────────────────────────────────────────────┤
│ Offset 64: Entry 1 (14 bytes)                  │
│   log_id = 0                                    │
│   timestamp = 123456789                         │
│   data_length = 0                               │
├─────────────────────────────────────────────────┤
│ Offset 78: Entry 2 (18 bytes)                  │
│   log_id = 1                                    │
│   timestamp = 123457000                         │
│   data_length = 4                               │
│   args: [05 00 00 00]  (int = 5)                │
├─────────────────────────────────────────────────┤
│ Offset 96: Entry 3 (22 bytes)                  │
│   log_id = 2                                    │
│   timestamp = 123458000                         │
│   data_length = 8                               │
│   args: [03 00 00 00] "bob" (len=3 + data)      │
├─────────────────────────────────────────────────┤
│ Offset 118: Dictionary (16 + 150 bytes)        │
│   magic = 0x44494354                            │
│   num_entries = 3                               │
│   [Entry definitions...]                        │
└─────────────────────────────────────────────────┘
Total file size: 284 bytes
```

---

## 5. Endianness Handling

### Problem
Different CPU architectures store multi-byte values differently:
- **Little-endian** (x86, x86-64, ARM64): Least significant byte first
- **Big-endian** (Some ARM, PowerPC): Most significant byte first

### Solution
1. **Writer**: Always writes in **native endianness** (for performance)
2. **Header**: Includes `endianness` field (0x01020304)
3. **Decompressor**: Detects and converts if needed

### Detection Logic
```c
void detect_endianness(cnanolog_file_header_t* header) {
    if (header->endianness == 0x01020304) {
        // Same endianness, no conversion needed
    } else if (header->endianness == 0x04030201) {
        // Different endianness, need to swap bytes
        header->timestamp_frequency = bswap_64(header->timestamp_frequency);
        // ... swap all multi-byte fields
    } else {
        fprintf(stderr, "Error: Invalid endianness marker\n");
    }
}
```

---

## 6. Size Limits

| Field | Type | Max Value | Notes |
|-------|------|-----------|-------|
| Log sites | uint32_t | 4,294,967,295 | Unique log statements |
| Arguments per log | uint8_t | 16 | Fixed array in dict entry |
| Argument data per entry | uint16_t | 65,535 bytes | Includes all args |
| String length | uint32_t | 4,294,967,295 | Per individual string |
| File size | uint64_t | ~16 EB | Effectively unlimited |
| Dictionary entries | uint32_t | 4,294,967,295 | Same as log sites |

---

## 7. Version Compatibility

### Version Scheme: MAJOR.MINOR

**MAJOR version increment (breaking change):**
- Changing field sizes
- Reordering fields
- Removing fields
- Changing magic numbers

**MINOR version increment (compatible change):**
- Adding new reserved fields (at end)
- Adding new argument types (new enum values)
- Adding optional metadata

### Decompressor Compatibility Rules

```c
if (header->version_major != EXPECTED_MAJOR) {
    fprintf(stderr, "Error: Incompatible major version %d (expected %d)\n",
            header->version_major, EXPECTED_MAJOR);
    return -1;
}

if (header->version_minor > EXPECTED_MINOR) {
    fprintf(stderr, "Warning: File created with newer minor version %d.%d\n",
            header->version_major, header->version_minor);
    // Continue - newer minor versions are backward compatible
}
```

---

## 8. Error Handling

### Invalid Data Detection

**Magic number mismatch:**
```c
if (header->magic != 0x4E414E4F) {
    fprintf(stderr, "Error: Not a CNanoLog file (magic = 0x%08X)\n",
            header->magic);
    return -1;
}
```

**Dictionary magic mismatch:**
```c
if (dict_header->magic != 0x44494354) {
    fprintf(stderr, "Error: Invalid dictionary magic 0x%08X\n",
            dict_header->magic);
    return -1;
}
```

**Entry data length overflow:**
```c
if (entry->data_length > MAX_ENTRY_SIZE) {
    fprintf(stderr, "Error: Entry data too large (%u bytes)\n",
            entry->data_length);
    return -1;
}
```

**Unknown log_id:**
```c
if (entry->log_id >= dict->num_entries) {
    fprintf(stderr, "Error: Unknown log_id %u\n", entry->log_id);
    return -1;
}
```

---

## 9. Implementation Notes

### Writing Order

1. **At init**: Write file header with placeholder values
2. **During runtime**: Append log entries sequentially
3. **At shutdown**:
   - Write dictionary at current file position
   - Seek back to header
   - Update `dictionary_offset` and `entry_count`
   - Flush and close

### Performance Considerations

1. **Buffered writes**: Use 64KB+ buffer to amortize write syscalls
2. **Sequential writes**: Never seek during logging (only at shutdown)
3. **Alignment**: Entry header is 14 bytes (not aligned), but this is okay for sequential access
4. **Padding**: No padding between entries (space efficiency)

### Thread Safety

- Each thread writes to its own staging buffer
- Background thread is the only one writing to file
- No synchronization needed for file writes

---

## 10. Future Extensions

### Possible Minor Version Additions (Backward Compatible)

**v1.1: Process metadata**
- Add process ID, thread ID to header
- Optional per-thread metadata

**v1.2: Compression metadata**
- Add compression algorithm field
- Support gzip/lz4 compressed sections

**v1.3: Checksum**
- Add CRC32 per entry or per block
- Detect corruption

### Reserved Fields
The 8 reserved bytes in the file header can be used for:
- Compression flags
- Checksum type
- Additional timestamp info
- Process/thread metadata

---

## 11. Summary

### Key Design Decisions

| Aspect | Choice | Rationale |
|--------|--------|-----------|
| Endianness | Native | Faster writes, conversion only at decompression |
| Dictionary location | End of file | No seeks during logging |
| String format | Length-prefixed | Faster than null-terminated for binary |
| Argument storage | Uncompressed | Simple, fast, compression comes later |
| Max arguments | 16 | Reasonable limit, keeps dict entry fixed size |
| Magic numbers | "NANO", "DICT" | Easy to identify in hex editor |

### File Size Examples

**1 million log entries:**
- No arguments: ~14 MB
- One integer: ~18 MB
- Two integers: ~22 MB
- One string (avg 10 chars): ~28 MB

**Compared to text (80 bytes/entry):** 80 MB
**Compression ratio:** 3-6x depending on argument types

---

## Appendix A: Complete Type Definitions

```c
// File: include/cnanolog_format.h

#ifndef CNANOLOG_FORMAT_H
#define CNANOLOG_FORMAT_H

#include <stdint.h>

// Magic numbers
#define CNANOLOG_MAGIC 0x4E414E4F  // "NANO"
#define CNANOLOG_DICT_MAGIC 0x44494354  // "DICT"

// Version
#define CNANOLOG_VERSION_MAJOR 1
#define CNANOLOG_VERSION_MINOR 0

// Limits
#define CNANOLOG_MAX_ARGS 16
#define CNANOLOG_MAX_ENTRY_SIZE 65535

// Argument types
typedef enum {
    ARG_TYPE_NONE    = 0,
    ARG_TYPE_INT32   = 1,
    ARG_TYPE_INT64   = 2,
    ARG_TYPE_UINT32  = 3,
    ARG_TYPE_UINT64  = 4,
    ARG_TYPE_DOUBLE  = 5,
    ARG_TYPE_STRING  = 6,
    ARG_TYPE_POINTER = 7,
} cnanolog_arg_type_t;

// File header
typedef struct {
    uint32_t magic;
    uint16_t version_major;
    uint16_t version_minor;
    uint64_t timestamp_frequency;
    uint64_t start_timestamp;
    int64_t  start_time_sec;
    int32_t  start_time_nsec;
    uint32_t endianness;
    uint64_t dictionary_offset;
    uint32_t entry_count;
    uint8_t  reserved[12];
} __attribute__((packed)) cnanolog_file_header_t;

// Entry header
typedef struct {
    uint32_t log_id;
    uint64_t timestamp;
    uint16_t data_length;
} __attribute__((packed)) cnanolog_entry_header_t;

// Dictionary header
typedef struct {
    uint32_t magic;
    uint32_t num_entries;
    uint32_t total_size;
    uint32_t reserved;
} __attribute__((packed)) cnanolog_dict_header_t;

// Dictionary entry
typedef struct {
    uint32_t log_id;
    uint8_t  log_level;
    uint8_t  num_args;
    uint16_t filename_length;
    uint16_t format_length;
    uint32_t line_number;
    uint8_t  arg_types[CNANOLOG_MAX_ARGS];
} __attribute__((packed)) cnanolog_dict_entry_t;

#endif // CNANOLOG_FORMAT_H
```

---

## Next Steps

With this format specification defined, we can now proceed to:

1. **Create header file** (`include/cnanolog_format.h`)
2. **Implement binary writer** (`src/binary_writer.c`)
3. **Implement decompressor** (`tools/decompressor.c`)
4. **Write format tests** (validate all struct sizes, field offsets)

The format is now locked and documented. Any implementation must match this specification exactly.
