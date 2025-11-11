/* Copyright (c) 2025
 * CNanoLog Decompressor
 *
 * Standalone tool to convert binary log files to human-readable text.
 * Usage: ./decompressor [options] <input.clog> [output.txt]
 *        If no output file specified, writes to stdout.
 */

#include "../include/cnanolog_format.h"
#include "../src/packer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

/* Default output format */
#define DEFAULT_FORMAT "[%t] [%l] [%f:%L] %m"

/* Maximum level filters */
#define MAX_LEVEL_FILTERS 64

/* ============================================================================
 * Dictionary Management
 * ============================================================================ */

typedef struct {
    uint8_t level;
    char name[32];
} level_entry_t;

typedef struct {
    uint32_t log_id;
    uint8_t log_level;
    uint8_t num_args;
    uint32_t line_number;
    char* filename;
    char* format;
    uint8_t arg_types[CNANOLOG_MAX_ARGS];
} dict_entry_t;

typedef struct {
    dict_entry_t* entries;
    uint32_t num_entries;
    level_entry_t* custom_levels;
    uint32_t num_custom_levels;
    uint64_t timestamp_frequency;
    uint64_t start_timestamp;
    time_t start_time_sec;
    int32_t start_time_nsec;
    int has_timestamps;  /* Flag: 1 if file contains timestamps, 0 otherwise */
} decompressor_ctx_t;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static const char* level_to_string(decompressor_ctx_t* ctx, uint8_t level) {
    /* First check built-in levels */
    switch (level) {
        case 0: return "INFO";
        case 1: return "WARN";
        case 2: return "ERROR";
        case 3: return "DEBUG";
        default: break;
    }

    /* Check custom levels */
    if (ctx != NULL && ctx->custom_levels != NULL) {
        for (uint32_t i = 0; i < ctx->num_custom_levels; i++) {
            if (ctx->custom_levels[i].level == level) {
                return ctx->custom_levels[i].name;
            }
        }
    }

    /* Unknown level - use static buffer to format number */
    static char buf[16];
    snprintf(buf, sizeof(buf), "LEVEL_%u", level);
    return buf;
}

/**
 * Convert rdtsc timestamp to human-readable time string.
 */
static void format_timestamp(decompressor_ctx_t* ctx, uint64_t timestamp, char* buf, size_t len) {
    /* Calculate elapsed time since start */
    uint64_t elapsed_ticks = timestamp - ctx->start_timestamp;
    double elapsed_seconds = (double)elapsed_ticks / ctx->timestamp_frequency;

    /* Calculate wall-clock time */
    time_t wall_time = ctx->start_time_sec + (time_t)elapsed_seconds;
    uint64_t nanos = (uint64_t)((elapsed_seconds - (time_t)elapsed_seconds) * 1000000000.0);

    /* Format: YYYY-MM-DD HH:MM:SS.nnnnnnnnn */
    struct tm* tm = localtime(&wall_time);
    snprintf(buf, len, "%04d-%02d-%02d %02d:%02d:%02d.%09llu",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec,
             (unsigned long long)nanos);
}

/* ============================================================================
 * Dictionary Loading
 * ============================================================================ */

/**
 * Load level dictionary from file (if present).
 * Returns 0 on success, -1 on failure.
 */
static int load_level_dictionary(FILE* fp, decompressor_ctx_t* ctx) {
    /* Read level dictionary header */
    cnanolog_level_dict_header_t level_header;
    if (fread(&level_header, 1, sizeof(level_header), fp) != sizeof(level_header)) {
        fprintf(stderr, "Error: Failed to read level dictionary header\n");
        return -1;
    }

    /* Validate level dictionary magic */
    if (level_header.magic != CNANOLOG_LEVEL_DICT_MAGIC) {
        /* Not a level dictionary - rewind and return success */
        fseek(fp, -(long)sizeof(level_header), SEEK_CUR);
        ctx->custom_levels = NULL;
        ctx->num_custom_levels = 0;
        return 0;
    }

    /* Allocate level entries */
    ctx->num_custom_levels = level_header.num_levels;
    if (ctx->num_custom_levels > 0) {
        ctx->custom_levels = (level_entry_t*)calloc(ctx->num_custom_levels, sizeof(level_entry_t));
        if (ctx->custom_levels == NULL) {
            fprintf(stderr, "Error: Failed to allocate level entries\n");
            return -1;
        }

        /* Read each level entry */
        for (uint32_t i = 0; i < ctx->num_custom_levels; i++) {
            cnanolog_level_dict_entry_t entry;
            if (fread(&entry, 1, sizeof(entry), fp) != sizeof(entry)) {
                fprintf(stderr, "Error: Failed to read level entry %u\n", i);
                return -1;
            }

            ctx->custom_levels[i].level = entry.level;

            /* Read level name */
            if (entry.name_length > sizeof(ctx->custom_levels[i].name) - 1) {
                fprintf(stderr, "Error: Level name too long\n");
                return -1;
            }

            if (fread(ctx->custom_levels[i].name, 1, entry.name_length, fp) != entry.name_length) {
                fprintf(stderr, "Error: Failed to read level name\n");
                return -1;
            }
            ctx->custom_levels[i].name[entry.name_length] = '\0';
        }
    }

    return 0;
}

/**
 * Load dictionary from file.
 * Returns 0 on success, -1 on failure.
 */
static int load_dictionary(FILE* fp, decompressor_ctx_t* ctx, uint64_t dict_offset) {
    /* Seek to dictionary */
    if (fseek(fp, dict_offset, SEEK_SET) != 0) {
        fprintf(stderr, "Error: Failed to seek to dictionary: %s\n", strerror(errno));
        return -1;
    }

    /* Try to load level dictionary first (optional) */
    if (load_level_dictionary(fp, ctx) != 0) {
        fprintf(stderr, "Error: Failed to load level dictionary\n");
        return -1;
    }

    /* Read log site dictionary header */
    cnanolog_dict_header_t dict_header;
    if (fread(&dict_header, 1, sizeof(dict_header), fp) != sizeof(dict_header)) {
        fprintf(stderr, "Error: Failed to read dictionary header\n");
        return -1;
    }

    /* Validate dictionary magic */
    if (cnanolog_validate_dict_header(&dict_header) != 0) {
        fprintf(stderr, "Error: Invalid dictionary magic: 0x%08X\n", dict_header.magic);
        return -1;
    }

    /* Allocate dictionary entries */
    ctx->num_entries = dict_header.num_entries;
    ctx->entries = (dict_entry_t*)calloc(ctx->num_entries, sizeof(dict_entry_t));
    if (ctx->entries == NULL) {
        fprintf(stderr, "Error: Failed to allocate dictionary entries\n");
        return -1;
    }

    /* Read each dictionary entry */
    for (uint32_t i = 0; i < ctx->num_entries; i++) {
        cnanolog_dict_entry_t entry;
        if (fread(&entry, 1, sizeof(entry), fp) != sizeof(entry)) {
            fprintf(stderr, "Error: Failed to read dictionary entry %u\n", i);
            return -1;
        }

        /* Copy fixed fields */
        ctx->entries[i].log_id = entry.log_id;
        ctx->entries[i].log_level = entry.log_level;
        ctx->entries[i].num_args = entry.num_args;
        ctx->entries[i].line_number = entry.line_number;
        memcpy(ctx->entries[i].arg_types, entry.arg_types, sizeof(entry.arg_types));

        /* Read filename */
        ctx->entries[i].filename = (char*)malloc(entry.filename_length + 1);
        if (ctx->entries[i].filename == NULL) {
            fprintf(stderr, "Error: Failed to allocate filename\n");
            return -1;
        }
        if (fread(ctx->entries[i].filename, 1, entry.filename_length, fp) != entry.filename_length) {
            fprintf(stderr, "Error: Failed to read filename\n");
            return -1;
        }
        ctx->entries[i].filename[entry.filename_length] = '\0';

        /* Read format string */
        ctx->entries[i].format = (char*)malloc(entry.format_length + 1);
        if (ctx->entries[i].format == NULL) {
            fprintf(stderr, "Error: Failed to allocate format string\n");
            return -1;
        }
        if (fread(ctx->entries[i].format, 1, entry.format_length, fp) != entry.format_length) {
            fprintf(stderr, "Error: Failed to read format string\n");
            return -1;
        }
        ctx->entries[i].format[entry.format_length] = '\0';
    }

    return 0;
}

/* ============================================================================
 * Argument Extraction
 * ============================================================================ */

/**
 * Count non-string arguments (for nibble calculation).
 */
static int count_non_string_args(const dict_entry_t* dict) {
    int count = 0;
    for (uint8_t i = 0; i < dict->num_args; i++) {
        if (dict->arg_types[i] != ARG_TYPE_STRING) {
            count++;
        }
    }
    return count;
}

/**
 * Decompress compressed argument data back to uncompressed format.
 * Returns number of uncompressed bytes written, or -1 on error.
 */
static int decompress_entry_args(const char* compressed,
                                  size_t compressed_len,
                                  char* uncompressed,
                                  size_t uncompressed_size,
                                  const dict_entry_t* dict) {
    const char* read_ptr = compressed;
    char* write_ptr = uncompressed;
    const char* end_ptr = compressed + compressed_len;
    const char* write_end = uncompressed + uncompressed_size;

    /* Calculate nibble size and read nibbles */
    int num_int_args = count_non_string_args(dict);
    size_t nibble_size = nibble_bytes(num_int_args);

    if (nibble_size > compressed_len) {
        return -1;  /* Invalid: not enough data for nibbles */
    }

    const uint8_t* nibbles = (const uint8_t*)read_ptr;
    read_ptr += nibble_size;

    /* ==================================================================
     * PASS 1: Read all integers into temporary storage
     * ================================================================== */

    /* Storage for unpacked integers (max 8 bytes each) */
    uint64_t int_values[CNANOLOG_MAX_ARGS];
    int nibble_idx = 0;
    int int_arg_idx = 0;

    for (uint8_t i = 0; i < dict->num_args; i++) {
        switch (dict->arg_types[i]) {
            case ARG_TYPE_CHAR: {
                if (read_ptr >= end_ptr) return -1;

                uint8_t nibble = get_nibble(nibbles, nibble_idx++);
                /* Nibble should be 1 for char */
                if (nibble != 1) return -1;

                char val;
                memcpy(&val, read_ptr, sizeof(char));
                read_ptr += sizeof(char);
                int_values[int_arg_idx++] = (uint64_t)(unsigned char)val;  /* Store as uint64 */
                break;
            }

            case ARG_TYPE_INT32: {
                if (read_ptr >= end_ptr) return -1;

                uint8_t nibble = get_nibble(nibbles, nibble_idx++);
                uint8_t num_bytes = nibble & 0x07;
                int is_negative = (nibble & 0x08) ? 1 : 0;

                if (num_bytes == 0 || num_bytes > 4) return -1;

                int32_t val = unpack_int32(&read_ptr, num_bytes, is_negative);
                int_values[int_arg_idx++] = (uint64_t)(uint32_t)val;  /* Store as uint64 */
                break;
            }

            case ARG_TYPE_INT64: {
                if (read_ptr >= end_ptr) return -1;

                uint8_t nibble = get_nibble(nibbles, nibble_idx++);
                uint8_t num_bytes = nibble & 0x07;
                int is_negative = (nibble & 0x08) ? 1 : 0;

                if (num_bytes == 0 || num_bytes > 8) return -1;

                int64_t val = unpack_int64(&read_ptr, num_bytes, is_negative);
                int_values[int_arg_idx++] = (uint64_t)val;
                break;
            }

            case ARG_TYPE_UINT32: {
                if (read_ptr >= end_ptr) return -1;

                uint8_t nibble = get_nibble(nibbles, nibble_idx++);
                uint8_t num_bytes = nibble & 0x0F;

                if (num_bytes == 0 || num_bytes > 4) return -1;

                uint32_t val = unpack_uint32(&read_ptr, num_bytes);
                int_values[int_arg_idx++] = (uint64_t)val;
                break;
            }

            case ARG_TYPE_UINT64: {
                if (read_ptr >= end_ptr) return -1;

                uint8_t nibble = get_nibble(nibbles, nibble_idx++);
                uint8_t num_bytes = nibble & 0x0F;

                if (num_bytes == 0 || num_bytes > 8) return -1;

                uint64_t val = unpack_uint64(&read_ptr, num_bytes);
                int_values[int_arg_idx++] = val;
                break;
            }

            case ARG_TYPE_DOUBLE: {
                if (read_ptr + sizeof(double) > end_ptr) return -1;

                nibble_idx++;

                /* Store double bits as uint64 */
                double d_val;
                memcpy(&d_val, read_ptr, sizeof(double));
                uint64_t bits;
                memcpy(&bits, &d_val, sizeof(uint64_t));
                int_values[int_arg_idx++] = bits;
                read_ptr += sizeof(double);
                break;
            }

            case ARG_TYPE_POINTER: {
                if (read_ptr >= end_ptr) return -1;

                uint8_t nibble = get_nibble(nibbles, nibble_idx++);
                uint8_t num_bytes = nibble & 0x0F;

                if (num_bytes == 0 || num_bytes > 8) return -1;

                uint64_t val = unpack_uint64(&read_ptr, num_bytes);
                int_values[int_arg_idx++] = val;
                break;
            }

            case ARG_TYPE_STRING:
                /* Skip - strings handled in pass 2 */
                break;

            default:
                return -1;
        }
    }

    /* ==================================================================
     * PASS 2: Write all arguments to uncompressed buffer in order
     * ================================================================== */

    int_arg_idx = 0;  /* Reset for writing */

    for (uint8_t i = 0; i < dict->num_args; i++) {
        switch (dict->arg_types[i]) {
            case ARG_TYPE_CHAR: {
                if (write_ptr + sizeof(char) > write_end) return -1;
                char val = (char)int_values[int_arg_idx++];
                memcpy(write_ptr, &val, sizeof(char));
                write_ptr += sizeof(char);
                break;
            }

            case ARG_TYPE_INT32: {
                if (write_ptr + sizeof(int32_t) > write_end) return -1;
                int32_t val = (int32_t)int_values[int_arg_idx++];
                memcpy(write_ptr, &val, sizeof(int32_t));
                write_ptr += sizeof(int32_t);
                break;
            }

            case ARG_TYPE_INT64: {
                if (write_ptr + sizeof(int64_t) > write_end) return -1;
                int64_t val = (int64_t)int_values[int_arg_idx++];
                memcpy(write_ptr, &val, sizeof(int64_t));
                write_ptr += sizeof(int64_t);
                break;
            }

            case ARG_TYPE_UINT32: {
                if (write_ptr + sizeof(uint32_t) > write_end) return -1;
                uint32_t val = (uint32_t)int_values[int_arg_idx++];
                memcpy(write_ptr, &val, sizeof(uint32_t));
                write_ptr += sizeof(uint32_t);
                break;
            }

            case ARG_TYPE_UINT64: {
                if (write_ptr + sizeof(uint64_t) > write_end) return -1;
                uint64_t val = int_values[int_arg_idx++];
                memcpy(write_ptr, &val, sizeof(uint64_t));
                write_ptr += sizeof(uint64_t);
                break;
            }

            case ARG_TYPE_DOUBLE: {
                if (write_ptr + sizeof(double) > write_end) return -1;
                uint64_t bits = int_values[int_arg_idx++];
                double val;
                memcpy(&val, &bits, sizeof(double));
                memcpy(write_ptr, &val, sizeof(double));
                write_ptr += sizeof(double);
                break;
            }

            case ARG_TYPE_POINTER: {
                if (write_ptr + sizeof(uint64_t) > write_end) return -1;
                uint64_t val = int_values[int_arg_idx++];
                memcpy(write_ptr, &val, sizeof(uint64_t));
                write_ptr += sizeof(uint64_t);
                break;
            }

            case ARG_TYPE_STRING: {
                if (read_ptr + sizeof(uint32_t) > end_ptr) return -1;

                uint32_t str_len;
                memcpy(&str_len, read_ptr, sizeof(uint32_t));
                read_ptr += sizeof(uint32_t);

                if (write_ptr + sizeof(uint32_t) > write_end) return -1;
                memcpy(write_ptr, &str_len, sizeof(uint32_t));
                write_ptr += sizeof(uint32_t);

                if (str_len > 0) {
                    if (read_ptr + str_len > end_ptr) return -1;
                    if (write_ptr + str_len > write_end) return -1;

                    memcpy(write_ptr, read_ptr, str_len);
                    read_ptr += str_len;
                    write_ptr += str_len;
                }
                break;
            }

            default:
                return -1;
        }
    }

    /* Validate that we consumed all compressed data */
    if (read_ptr != end_ptr) {
        return -1;  /* Didn't consume exact amount - likely uncompressed */
    }

    return (int)(write_ptr - uncompressed);
}

/**
 * Extract arguments from binary data and format the log message.
 */
static void format_log_message(decompressor_ctx_t* ctx, dict_entry_t* dict,
                                const char* arg_data, char* output, size_t output_size) {
    const char* read_ptr = arg_data;
    char formatted[2048];
    char* write_ptr = formatted;
    const char* fmt_ptr = dict->format;
    int arg_index = 0;

    /* Process format string */
    while (*fmt_ptr && (write_ptr - formatted) < (int)sizeof(formatted) - 1) {
        if (*fmt_ptr == '%' && *(fmt_ptr + 1) != '%') {
            /* Found format specifier - extract argument */
            if (arg_index >= dict->num_args) {
                /* No more arguments */
                *write_ptr++ = *fmt_ptr++;
                continue;
            }

            cnanolog_arg_type_t arg_type = (cnanolog_arg_type_t)dict->arg_types[arg_index];
            arg_index++;

            /* Skip the % and any flags/width/precision */
            fmt_ptr++;
            while (*fmt_ptr && strchr("-+ #0123456789.*", *fmt_ptr)) {
                fmt_ptr++;
            }

            /* Skip the conversion specifier */
            if (*fmt_ptr) fmt_ptr++;

            /* Extract and format argument based on type */
            switch (arg_type) {
                case ARG_TYPE_CHAR: {
                    char val;
                    memcpy(&val, read_ptr, sizeof(val));
                    read_ptr += sizeof(val);
                    write_ptr += snprintf(write_ptr,
                                         sizeof(formatted) - (write_ptr - formatted),
                                         "%c", (int)val);
                    break;
                }
                case ARG_TYPE_INT32: {
                    int32_t val;
                    memcpy(&val, read_ptr, sizeof(val));
                    read_ptr += sizeof(val);
                    write_ptr += snprintf(write_ptr,
                                         sizeof(formatted) - (write_ptr - formatted),
                                         "%d", val);
                    break;
                }
                case ARG_TYPE_INT64: {
                    int64_t val;
                    memcpy(&val, read_ptr, sizeof(val));
                    read_ptr += sizeof(val);
                    write_ptr += snprintf(write_ptr,
                                         sizeof(formatted) - (write_ptr - formatted),
                                         "%lld", (long long)val);
                    break;
                }
                case ARG_TYPE_UINT32: {
                    uint32_t val;
                    memcpy(&val, read_ptr, sizeof(val));
                    read_ptr += sizeof(val);
                    write_ptr += snprintf(write_ptr,
                                         sizeof(formatted) - (write_ptr - formatted),
                                         "%u", val);
                    break;
                }
                case ARG_TYPE_UINT64: {
                    uint64_t val;
                    memcpy(&val, read_ptr, sizeof(val));
                    read_ptr += sizeof(val);
                    write_ptr += snprintf(write_ptr,
                                         sizeof(formatted) - (write_ptr - formatted),
                                         "%llu", (unsigned long long)val);
                    break;
                }
                case ARG_TYPE_DOUBLE: {
                    double val;
                    memcpy(&val, read_ptr, sizeof(val));
                    read_ptr += sizeof(val);
                    write_ptr += snprintf(write_ptr,
                                         sizeof(formatted) - (write_ptr - formatted),
                                         "%f", val);
                    break;
                }
                case ARG_TYPE_STRING: {
                    uint32_t str_len;
                    memcpy(&str_len, read_ptr, sizeof(str_len));
                    read_ptr += sizeof(str_len);

                    /* Copy string (it's not null-terminated in binary) */
                    int copy_len = str_len;
                    if (write_ptr + copy_len >= formatted + sizeof(formatted)) {
                        copy_len = (formatted + sizeof(formatted) - 1) - write_ptr;
                    }
                    memcpy(write_ptr, read_ptr, copy_len);
                    write_ptr += copy_len;
                    read_ptr += str_len;
                    break;
                }
                case ARG_TYPE_POINTER: {
                    uint64_t val;
                    memcpy(&val, read_ptr, sizeof(val));
                    read_ptr += sizeof(val);
                    write_ptr += snprintf(write_ptr,
                                         sizeof(formatted) - (write_ptr - formatted),
                                         "%p", (void*)val);
                    break;
                }
                default:
                    /* Unknown type, skip */
                    break;
            }
        } else {
            /* Regular character or %% */
            *write_ptr++ = *fmt_ptr++;
            if (*(fmt_ptr - 1) == '%' && *fmt_ptr == '%') {
                fmt_ptr++; /* Skip second % */
            }
        }
    }
    *write_ptr = '\0';

    /* Copy to output */
    snprintf(output, output_size, "%s", formatted);
}

/* ============================================================================
 * Output Formatting
 * ============================================================================ */

/**
 * Format a log entry according to the specified format string.
 *
 * Format tokens:
 *   %t - timestamp (human-readable)
 *   %T - timestamp (raw ticks)
 *   %r - relative time since start (seconds)
 *   %l - log level
 *   %f - filename
 *   %L - line number
 *   %m - formatted message
 *   %% - literal %
 */
static void format_output(const char* format,
                          const char* timestamp_str,
                          uint64_t timestamp_raw,
                          decompressor_ctx_t* ctx,
                          dict_entry_t* dict,
                          const char* message,
                          char* output,
                          size_t output_size) {
    const char* fmt_ptr = format;
    char* out_ptr = output;
    char* out_end = output + output_size - 1;

    while (*fmt_ptr && out_ptr < out_end) {
        if (*fmt_ptr == '%') {
            fmt_ptr++;
            switch (*fmt_ptr) {
                case 't': {  /* Human-readable timestamp */
                    int written = snprintf(out_ptr, out_end - out_ptr, "%s", timestamp_str);
                    out_ptr += (written > 0) ? written : 0;
                    fmt_ptr++;
                    break;
                }
                case 'T': {  /* Raw timestamp ticks */
                    int written = snprintf(out_ptr, out_end - out_ptr, "%llu",
                                          (unsigned long long)timestamp_raw);
                    out_ptr += (written > 0) ? written : 0;
                    fmt_ptr++;
                    break;
                }
                case 'r': {  /* Relative time in seconds */
                    uint64_t elapsed_ticks = timestamp_raw - ctx->start_timestamp;
                    double elapsed_seconds = (double)elapsed_ticks / ctx->timestamp_frequency;
                    int written = snprintf(out_ptr, out_end - out_ptr, "%.9f", elapsed_seconds);
                    out_ptr += (written > 0) ? written : 0;
                    fmt_ptr++;
                    break;
                }
                case 'l': {  /* Log level */
                    int written = snprintf(out_ptr, out_end - out_ptr, "%s",
                                          level_to_string(ctx, dict->log_level));
                    out_ptr += (written > 0) ? written : 0;
                    fmt_ptr++;
                    break;
                }
                case 'f': {  /* Filename */
                    int written = snprintf(out_ptr, out_end - out_ptr, "%s", dict->filename);
                    out_ptr += (written > 0) ? written : 0;
                    fmt_ptr++;
                    break;
                }
                case 'L': {  /* Line number */
                    int written = snprintf(out_ptr, out_end - out_ptr, "%u", dict->line_number);
                    out_ptr += (written > 0) ? written : 0;
                    fmt_ptr++;
                    break;
                }
                case 'm': {  /* Message */
                    int written = snprintf(out_ptr, out_end - out_ptr, "%s", message);
                    out_ptr += (written > 0) ? written : 0;
                    fmt_ptr++;
                    break;
                }
                case '%': {  /* Literal % */
                    if (out_ptr < out_end) {
                        *out_ptr++ = '%';
                    }
                    fmt_ptr++;
                    break;
                }
                default: {  /* Unknown format, copy as-is */
                    if (out_ptr < out_end) {
                        *out_ptr++ = '%';
                    }
                    if (out_ptr < out_end && *fmt_ptr) {
                        *out_ptr++ = *fmt_ptr++;
                    }
                    break;
                }
            }
        } else {
            *out_ptr++ = *fmt_ptr++;
        }
    }
    *out_ptr = '\0';
}

/* ============================================================================
 * Level Filtering
 * ============================================================================ */

/**
 * Parse comma-separated level names into level numbers.
 * Returns number of levels parsed, or -1 on error.
 */
static int parse_level_filters(const char* filter_str, decompressor_ctx_t* ctx,
                               uint8_t* levels_out, int max_levels) {
    if (filter_str == NULL || levels_out == NULL) {
        return -1;
    }

    char* str_copy = strdup(filter_str);
    if (str_copy == NULL) {
        return -1;
    }

    int count = 0;
    char* token = strtok(str_copy, ",");
    while (token != NULL && count < max_levels) {
        /* Trim whitespace */
        while (*token == ' ' || *token == '\t') token++;
        char* end = token + strlen(token) - 1;
        while (end > token && (*end == ' ' || *end == '\t')) {
            *end = '\0';
            end--;
        }

        /* Try to match built-in levels */
        uint8_t level = 255;  /* Invalid */
        if (strcasecmp(token, "INFO") == 0) {
            level = 0;
        } else if (strcasecmp(token, "WARN") == 0) {
            level = 1;
        } else if (strcasecmp(token, "ERROR") == 0) {
            level = 2;
        } else if (strcasecmp(token, "DEBUG") == 0) {
            level = 3;
        } else {
            /* Try to match custom levels */
            if (ctx->custom_levels != NULL) {
                for (uint32_t i = 0; i < ctx->num_custom_levels; i++) {
                    if (strcasecmp(token, ctx->custom_levels[i].name) == 0) {
                        level = ctx->custom_levels[i].level;
                        break;
                    }
                }
            }
        }

        if (level == 255) {
            fprintf(stderr, "Warning: Unknown level '%s', ignoring\n", token);
        } else {
            levels_out[count++] = level;
        }

        token = strtok(NULL, ",");
    }

    free(str_copy);
    return count;
}

/**
 * Check if a level should be included based on filter.
 * Returns 1 if should include, 0 if should filter out.
 */
static int should_include_level(uint8_t level, const uint8_t* filter_levels, int num_filters) {
    if (filter_levels == NULL || num_filters == 0) {
        return 1;  /* No filter, include everything */
    }

    for (int i = 0; i < num_filters; i++) {
        if (level == filter_levels[i]) {
            return 1;
        }
    }
    return 0;
}

/* ============================================================================
 * Help and Usage
 * ============================================================================ */

static void print_help(const char* program_name) {
    fprintf(stderr, "CNanoLog Decompressor - Convert binary log files to text\n\n");
    fprintf(stderr, "Usage: %s [options] <input.clog> [output.txt]\n\n", program_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -f, --format <fmt>   Specify output format (default: \"[%%t] [%%l] [%%f:%%L] %%m\")\n");
    fprintf(stderr, "  -l, --level <levels> Filter by log level (comma-separated, e.g., \"METRIC,AUDIT\")\n");
    fprintf(stderr, "  -h, --help           Show this help message\n\n");
    fprintf(stderr, "Format tokens:\n");
    fprintf(stderr, "  %%t   Human-readable timestamp (YYYY-MM-DD HH:MM:SS.nnnnnnnnn)\n");
    fprintf(stderr, "  %%T   Raw timestamp (CPU ticks)\n");
    fprintf(stderr, "  %%r   Relative time since start (seconds with nanosecond precision)\n");
    fprintf(stderr, "  %%l   Log level (INFO, WARN, ERROR, DEBUG)\n");
    fprintf(stderr, "  %%f   Source filename\n");
    fprintf(stderr, "  %%L   Line number\n");
    fprintf(stderr, "  %%m   Formatted log message\n");
    fprintf(stderr, "  %%%%   Literal %% character\n\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  # Default format\n");
    fprintf(stderr, "  %s app.clog\n\n", program_name);
    fprintf(stderr, "  # Custom format: only timestamp and message\n");
    fprintf(stderr, "  %s -f \"%%t: %%m\" app.clog\n\n", program_name);
    fprintf(stderr, "  # CSV format\n");
    fprintf(stderr, "  %s -f \"%%t,%%l,%%f,%%L,%%m\" app.clog app.csv\n\n", program_name);
    fprintf(stderr, "  # JSON-like format\n");
    fprintf(stderr, "  %s -f '{\"time\":\"%%t\",\"level\":\"%%l\",\"msg\":\"%%m\"}' app.clog\n\n", program_name);
    fprintf(stderr, "If output file is not specified, writes to stdout.\n");
}

/* ============================================================================
 * Main Decompression
 * ============================================================================ */

static int decompress_file(const char* input_path, FILE* output_fp, const char* output_format,
                          const char* level_filter_str) {
    FILE* input_fp = NULL;
    decompressor_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    uint8_t filter_levels[MAX_LEVEL_FILTERS];
    int num_filter_levels = 0;
    int ret = -1;

    /* Open input file */
    input_fp = fopen(input_path, "rb");
    if (input_fp == NULL) {
        fprintf(stderr, "Error: Cannot open input file '%s': %s\n",
                input_path, strerror(errno));
        return -1;
    }

    /* Read file header */
    cnanolog_file_header_t header;
    if (fread(&header, 1, sizeof(header), input_fp) != sizeof(header)) {
        fprintf(stderr, "Error: Failed to read file header\n");
        goto cleanup;
    }

    /* Validate header */
    if (cnanolog_validate_file_header(&header) != 0) {
        fprintf(stderr, "Error: Invalid file header (magic: 0x%08X)\n", header.magic);
        goto cleanup;
    }

    /* Check endianness */
    int endian_check = cnanolog_check_endianness(header.endianness);
    if (endian_check == -1) {
        fprintf(stderr, "Error: Invalid endianness marker: 0x%08X\n", header.endianness);
        goto cleanup;
    }
    if (endian_check == 1) {
        fprintf(stderr, "Warning: File uses different endianness (byte swap not implemented yet)\n");
        /* TODO: Implement byte swapping */
    }

    /* Store timing info */
    ctx.timestamp_frequency = header.timestamp_frequency;
    ctx.start_timestamp = header.start_timestamp;
    ctx.start_time_sec = header.start_time_sec;
    ctx.start_time_nsec = header.start_time_nsec;

    /* Check if file has timestamps */
    ctx.has_timestamps = (header.flags & CNANOLOG_FLAG_HAS_TIMESTAMPS) != 0;

    /* Determine dictionary offset */
    uint64_t dict_offset;
    if (header.dictionary_offset == 0) {
        /* Dictionary at end of file - not yet supported */
        fprintf(stderr, "Error: Dictionary offset is 0 (not yet supported)\n");
        goto cleanup;
    } else {
        dict_offset = header.dictionary_offset;
    }

    /* Load dictionary */
    if (load_dictionary(input_fp, &ctx, dict_offset) != 0) {
        fprintf(stderr, "Error: Failed to load dictionary\n");
        goto cleanup;
    }

    /* Parse level filters (now that we have custom levels loaded) */
    if (level_filter_str != NULL) {
        num_filter_levels = parse_level_filters(level_filter_str, &ctx,
                                                filter_levels, MAX_LEVEL_FILTERS);
        if (num_filter_levels < 0) {
            fprintf(stderr, "Error: Failed to parse level filter\n");
            goto cleanup;
        }
        if (num_filter_levels > 0) {
            fprintf(stderr, "Filtering by %d level(s)\n", num_filter_levels);
        }
    }

    /* Seek back to first entry (after header) */
    if (fseek(input_fp, sizeof(header), SEEK_SET) != 0) {
        fprintf(stderr, "Error: Failed to seek to entries\n");
        goto cleanup;
    }

    /* Decompress entries */
    uint32_t entries_processed = 0;
    char arg_buffer[CNANOLOG_MAX_ENTRY_SIZE];
    char uncompressed_buffer[CNANOLOG_MAX_ENTRY_SIZE];

    while (entries_processed < header.entry_count) {
        /* Read entry header - format depends on whether timestamps are enabled */
        uint32_t log_id;
        uint64_t timestamp = 0;
        uint16_t data_length;

        /* Read log_id (always 4 bytes) */
        if (fread(&log_id, 1, sizeof(log_id), input_fp) != sizeof(log_id)) {
            if (feof(input_fp)) break;  /* EOF */
            fprintf(stderr, "Error: Failed to read entry log_id\n");
            goto cleanup;
        }

        /* Read timestamp (8 bytes) if enabled */
        if (ctx.has_timestamps) {
            if (fread(&timestamp, 1, sizeof(timestamp), input_fp) != sizeof(timestamp)) {
                fprintf(stderr, "Error: Failed to read entry timestamp\n");
                goto cleanup;
            }
        }

        /* Read data_length (always 2 bytes) */
        if (fread(&data_length, 1, sizeof(data_length), input_fp) != sizeof(data_length)) {
            fprintf(stderr, "Error: Failed to read entry data_length\n");
            goto cleanup;
        }

        /* Validate log_id */
        if (log_id >= ctx.num_entries) {
            fprintf(stderr, "Error: Invalid log_id %u (max %u)\n",
                    log_id, ctx.num_entries - 1);
            goto cleanup;
        }

        /* Read argument data (compressed) */
        if (data_length > 0) {
            if (fread(arg_buffer, 1, data_length, input_fp) != data_length) {
                fprintf(stderr, "Error: Failed to read entry data\n");
                goto cleanup;
            }
        }

        /* Format timestamp (if present) */
        char timestamp_str[64];
        if (ctx.has_timestamps) {
            format_timestamp(&ctx, timestamp, timestamp_str, sizeof(timestamp_str));
        } else {
            snprintf(timestamp_str, sizeof(timestamp_str), "NO-TIMESTAMP");
        }

        /* Get dictionary entry */
        dict_entry_t* dict = &ctx.entries[log_id];

        /* Apply level filter */
        if (!should_include_level(dict->log_level, filter_levels, num_filter_levels)) {
            entries_processed++;
            continue;  /* Skip this entry */
        }

        /* Decompress argument data */
        const char* data_to_format = arg_buffer;
        if (data_length > 0) {
            int decompressed_len = decompress_entry_args(
                arg_buffer,
                data_length,
                uncompressed_buffer,
                sizeof(uncompressed_buffer),
                dict);

            if (decompressed_len > 0) {
                /* Use decompressed data */
                data_to_format = uncompressed_buffer;
            }
            /* If decompression fails, fall back to treating as uncompressed */
        }

        /* Format message */
        char message[2048];
        format_log_message(&ctx, dict, data_to_format, message, sizeof(message));

        /* Format and output log line according to output format */
        char formatted_line[4096];
        format_output(output_format, timestamp_str, timestamp, &ctx, dict, message,
                     formatted_line, sizeof(formatted_line));
        fprintf(output_fp, "%s\n", formatted_line);

        entries_processed++;
    }

    fprintf(stderr, "Decompressed %u entries\n", entries_processed);
    ret = 0;

cleanup:
    /* Free dictionary */
    if (ctx.entries != NULL) {
        for (uint32_t i = 0; i < ctx.num_entries; i++) {
            free(ctx.entries[i].filename);
            free(ctx.entries[i].format);
        }
        free(ctx.entries);
    }

    /* Free custom levels */
    if (ctx.custom_levels != NULL) {
        free(ctx.custom_levels);
    }

    if (input_fp != NULL) {
        fclose(input_fp);
    }

    return ret;
}

/* ============================================================================
 * Main Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    const char* input_path = NULL;
    const char* output_path = NULL;
    const char* output_format = DEFAULT_FORMAT;
    const char* level_filter_str = NULL;
    FILE* output_fp = stdout;

    /* Parse command-line arguments */
    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--format") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: %s requires an argument\n", argv[i]);
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                return 1;
            }
            output_format = argv[i + 1];
            i += 2;
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--level") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: %s requires an argument\n", argv[i]);
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                return 1;
            }
            level_filter_str = argv[i + 1];
            i += 2;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            return 1;
        } else {
            /* Positional argument */
            if (input_path == NULL) {
                input_path = argv[i];
            } else if (output_path == NULL) {
                output_path = argv[i];
            } else {
                fprintf(stderr, "Error: Too many arguments\n");
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                return 1;
            }
            i++;
        }
    }

    /* Validate required arguments */
    if (input_path == NULL) {
        fprintf(stderr, "Error: No input file specified\n");
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    /* Open output file if specified */
    if (output_path != NULL) {
        output_fp = fopen(output_path, "w");
        if (output_fp == NULL) {
            fprintf(stderr, "Error: Cannot open output file '%s': %s\n",
                    output_path, strerror(errno));
            return 1;
        }
    }

    /* Decompress */
    int ret = decompress_file(input_path, output_fp, output_format, level_filter_str);

    /* Close output file */
    if (output_fp != stdout) {
        fclose(output_fp);
    }

    return ret;
}
