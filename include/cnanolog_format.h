/* Copyright (c) 2025
 * CNanoLog Binary Format Definitions
 *
 * This file defines the binary log file format structures.
 * See docs/BINARY_FORMAT_SPEC.md for detailed specification.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Compatibility macro for static assertions */
#ifdef __cplusplus
    #define CNANOLOG_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
    #define CNANOLOG_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

/* ============================================================================
 * Magic Numbers and Version
 * ============================================================================ */

#define CNANOLOG_MAGIC      0x4E414E4F  /* "NANO" in ASCII */
#define CNANOLOG_DICT_MAGIC 0x44494354  /* "DICT" in ASCII */

#define CNANOLOG_VERSION_MAJOR 1
#define CNANOLOG_VERSION_MINOR 0

/* ============================================================================
 * Limits
 * ============================================================================ */

#define CNANOLOG_MAX_ARGS       50      /* Maximum arguments per log statement */
#define CNANOLOG_MAX_ENTRY_SIZE 65535   /* Maximum size of entry data (uint16_t) */

/* ============================================================================
 * Argument Type Codes
 * ============================================================================ */

typedef enum {
    ARG_TYPE_NONE    = 0,   /* No argument (placeholder) */
    ARG_TYPE_INT32   = 1,   /* int32_t, int */
    ARG_TYPE_INT64   = 2,   /* int64_t, long, long long */
    ARG_TYPE_UINT32  = 3,   /* uint32_t, unsigned int */
    ARG_TYPE_UINT64  = 4,   /* uint64_t, unsigned long */
    ARG_TYPE_DOUBLE  = 5,   /* double, float (promoted) */
    ARG_TYPE_STRING  = 6,   /* char*, const char* */
    ARG_TYPE_POINTER = 7,   /* void*, any pointer type */
} cnanolog_arg_type_t;

/* ============================================================================
 * File Header Flags
 * ============================================================================ */

#define CNANOLOG_FLAG_HAS_TIMESTAMPS  0x00000001  /* Entries include timestamps */

/* ============================================================================
 * File Header (64 bytes)
 * ============================================================================ */

/**
 * File header at the beginning of every CNanoLog file.
 * Total size: 64 bytes (fixed)
 */
typedef struct {
    uint32_t magic;              /* Magic number: 0x4E414E4F ("NANO") */
    uint16_t version_major;      /* Format version major (currently 1) */
    uint16_t version_minor;      /* Format version minor (currently 0) */
    uint64_t timestamp_frequency; /* CPU ticks per second (rdtsc frequency, 0 if timestamps disabled) */
    uint64_t start_timestamp;    /* rdtsc() value when logging started (0 if timestamps disabled) */
    int64_t  start_time_sec;     /* Unix epoch seconds when logging started */
    int32_t  start_time_nsec;    /* Nanoseconds component (0-999999999) */
    uint32_t endianness;         /* Always 0x01020304 for endian detection */
    uint64_t dictionary_offset;  /* Byte offset to dictionary (0 = end of file) */
    uint32_t entry_count;        /* Total number of log entries written */
    uint32_t flags;              /* Feature flags (see CNANOLOG_FLAG_*) */
    uint8_t  reserved[8];        /* Reserved for future use (must be 0) */
} __attribute__((packed)) cnanolog_file_header_t;

/* Compile-time size check */
CNANOLOG_STATIC_ASSERT(sizeof(cnanolog_file_header_t) == 64,
                       "File header must be exactly 64 bytes");

/* ============================================================================
 * Log Entry Header (14 bytes with timestamps, 6 bytes without)
 * ============================================================================ */

/**
 * Header for each log entry in the file.
 * Followed by data_length bytes of argument data.
 *
 * Size depends on CNANOLOG_NO_TIMESTAMPS:
 * - With timestamps (default): 14 bytes (log_id + timestamp + data_length)
 * - Without timestamps: 6 bytes (log_id + data_length)
 */
#ifdef CNANOLOG_NO_TIMESTAMPS
    typedef struct {
        uint32_t log_id;        /* Log site identifier (index into dictionary) */
        uint16_t data_length;   /* Number of bytes of argument data following */
    } __attribute__((packed)) cnanolog_entry_header_t;

    /* Compile-time size check */
    CNANOLOG_STATIC_ASSERT(sizeof(cnanolog_entry_header_t) == 6,
                           "Entry header (no timestamps) must be exactly 6 bytes");
#else
    typedef struct {
        uint32_t log_id;        /* Log site identifier (index into dictionary) */
        uint64_t timestamp;     /* rdtsc() value when log was created */
        uint16_t data_length;   /* Number of bytes of argument data following */
    } __attribute__((packed)) cnanolog_entry_header_t;

    /* Compile-time size check */
    CNANOLOG_STATIC_ASSERT(sizeof(cnanolog_entry_header_t) == 14,
                           "Entry header (with timestamps) must be exactly 14 bytes");
#endif

/* ============================================================================
 * Dictionary Header (16 bytes)
 * ============================================================================ */

/**
 * Header for the dictionary section at the end of the file.
 * Followed by num_entries dictionary entries.
 */
typedef struct {
    uint32_t magic;         /* Magic number: 0x44494354 ("DICT") */
    uint32_t num_entries;   /* Number of log site entries in dictionary */
    uint32_t total_size;    /* Total size of dictionary section in bytes */
    uint32_t reserved;      /* Reserved for future use (must be 0) */
} __attribute__((packed)) cnanolog_dict_header_t;

/* Compile-time size check */
CNANOLOG_STATIC_ASSERT(sizeof(cnanolog_dict_header_t) == 16,
                       "Dictionary header must be exactly 16 bytes");

/* ============================================================================
 * Dictionary Entry (30 bytes + variable strings)
 * ============================================================================ */

/**
 * Entry in the dictionary describing a log site.
 * Followed by:
 *   - filename string (filename_length bytes, no null terminator)
 *   - format string (format_length bytes, no null terminator)
 */
typedef struct {
    uint32_t log_id;            /* Unique log site identifier */
    uint8_t  log_level;         /* Log level (0=DEBUG, 1=INFO, 2=WARN, 3=ERROR) */
    uint8_t  num_args;          /* Number of format arguments (0-16) */
    uint16_t filename_length;   /* Length of filename string (bytes) */
    uint16_t format_length;     /* Length of format string (bytes) */
    uint32_t line_number;       /* Line number in source file */
    uint8_t  arg_types[CNANOLOG_MAX_ARGS]; /* Type code for each argument */
} __attribute__((packed)) cnanolog_dict_entry_t;

/* Compile-time size check */
CNANOLOG_STATIC_ASSERT(sizeof(cnanolog_dict_entry_t) == 64,
                       "Dictionary entry must be exactly 64 bytes (14 fixed + 50 arg_types)");

/* ============================================================================
 * Level Dictionary (for custom log levels)
 * ============================================================================ */

#define CNANOLOG_LEVEL_DICT_MAGIC 0x4C564C53  /* "LVLS" in ASCII */

/**
 * Header for the level dictionary section.
 * Written before the log site dictionary if custom levels are registered.
 */
typedef struct {
    uint32_t magic;         /* Magic number: 0x4C564C53 ("LVLS") */
    uint32_t num_levels;    /* Number of custom level entries */
    uint32_t total_size;    /* Total size of level dictionary in bytes */
    uint32_t reserved;      /* Reserved for future use (must be 0) */
} __attribute__((packed)) cnanolog_level_dict_header_t;

/* Compile-time size check */
CNANOLOG_STATIC_ASSERT(sizeof(cnanolog_level_dict_header_t) == 16,
                       "Level dictionary header must be exactly 16 bytes");

/**
 * Entry for a custom log level.
 * Maps a level value to its name.
 */
typedef struct {
    uint8_t level;          /* Level value (0-255) */
    uint8_t name_length;    /* Length of level name string */
    uint8_t reserved[2];    /* Reserved for alignment (must be 0) */
} __attribute__((packed)) cnanolog_level_dict_entry_t;

/* Compile-time size check */
CNANOLOG_STATIC_ASSERT(sizeof(cnanolog_level_dict_entry_t) == 4,
                       "Level dictionary entry must be exactly 4 bytes");

/* ============================================================================
 * Helper Macros
 * ============================================================================ */

/* Endianness detection value (little-endian: 04 03 02 01) */
#define CNANOLOG_ENDIAN_MAGIC 0x01020304

/* Calculate total entry size including header */
#define CNANOLOG_ENTRY_TOTAL_SIZE(data_len) \
    (sizeof(cnanolog_entry_header_t) + (data_len))

/* Calculate total dict entry size including strings */
#define CNANOLOG_DICT_ENTRY_TOTAL_SIZE(filename_len, format_len) \
    (sizeof(cnanolog_dict_entry_t) + (filename_len) + (format_len))

/* ============================================================================
 * Validation Functions
 * ============================================================================ */

/**
 * Validate that a file header is correct.
 * Returns 0 if valid, -1 if invalid.
 */
static inline int cnanolog_validate_file_header(const cnanolog_file_header_t* header) {
    if (header->magic != CNANOLOG_MAGIC) {
        return -1;  /* Invalid magic number */
    }
    if (header->version_major != CNANOLOG_VERSION_MAJOR) {
        return -1;  /* Incompatible major version */
    }
    /* Note: Minor version can be higher (backward compatible) */
    return 0;
}

/**
 * Validate that a dictionary header is correct.
 * Returns 0 if valid, -1 if invalid.
 */
static inline int cnanolog_validate_dict_header(const cnanolog_dict_header_t* header) {
    if (header->magic != CNANOLOG_DICT_MAGIC) {
        return -1;  /* Invalid magic number */
    }
    return 0;
}

/**
 * Check if endianness conversion is needed.
 * Returns 1 if conversion needed, 0 if same endian, -1 if invalid.
 */
static inline int cnanolog_check_endianness(uint32_t endian_marker) {
    if (endian_marker == CNANOLOG_ENDIAN_MAGIC) {
        return 0;  /* Same endianness */
    } else if (endian_marker == 0x04030201) {
        return 1;  /* Different endianness, need byte swap */
    } else {
        return -1;  /* Invalid marker */
    }
}

/* ============================================================================
 * Byte Swap Utilities (for endianness conversion)
 * ============================================================================ */

#if defined(__GNUC__) || defined(__clang__)
    #define cnanolog_bswap16(x) __builtin_bswap16(x)
    #define cnanolog_bswap32(x) __builtin_bswap32(x)
    #define cnanolog_bswap64(x) __builtin_bswap64(x)
#elif defined(_MSC_VER)
    #include <stdlib.h>
    #define cnanolog_bswap16(x) _byteswap_ushort(x)
    #define cnanolog_bswap32(x) _byteswap_ulong(x)
    #define cnanolog_bswap64(x) _byteswap_uint64(x)
#else
    /* Fallback implementation */
    static inline uint16_t cnanolog_bswap16(uint16_t x) {
        return (x >> 8) | (x << 8);
    }
    static inline uint32_t cnanolog_bswap32(uint32_t x) {
        return ((x >> 24) & 0x000000FF) |
               ((x >>  8) & 0x0000FF00) |
               ((x <<  8) & 0x00FF0000) |
               ((x << 24) & 0xFF000000);
    }
    static inline uint64_t cnanolog_bswap64(uint64_t x) {
        return ((x >> 56) & 0x00000000000000FFULL) |
               ((x >> 40) & 0x000000000000FF00ULL) |
               ((x >> 24) & 0x0000000000FF0000ULL) |
               ((x >>  8) & 0x00000000FF000000ULL) |
               ((x <<  8) & 0x000000FF00000000ULL) |
               ((x << 24) & 0x0000FF0000000000ULL) |
               ((x << 40) & 0x00FF000000000000ULL) |
               ((x << 56) & 0xFF00000000000000ULL);
    }
#endif

#ifdef __cplusplus
}
#endif
