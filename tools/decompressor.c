/* Copyright (c) 2025
 * CNanoLog Decompressor
 *
 * Standalone tool to convert binary log files to human-readable text.
 * Usage: ./decompressor <input.clog> [output.txt]
 *        If no output file specified, writes to stdout.
 */

#include "../include/cnanolog_format.h"
#include "../src/packer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

/* ============================================================================
 * Dictionary Management
 * ============================================================================ */

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
    uint64_t timestamp_frequency;
    uint64_t start_timestamp;
    time_t start_time_sec;
    int32_t start_time_nsec;
    int has_timestamps;  /* Flag: 1 if file contains timestamps, 0 otherwise */
} decompressor_ctx_t;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static const char* level_to_string(uint8_t level) {
    /* Match the order in cnanolog.h:
     * LOG_LEVEL_INFO  = 0
     * LOG_LEVEL_WARN  = 1
     * LOG_LEVEL_ERROR = 2
     * LOG_LEVEL_DEBUG = 3
     */
    switch (level) {
        case 0: return "INFO";
        case 1: return "WARN";
        case 2: return "ERROR";
        case 3: return "DEBUG";
        default: return "UNKNOWN";
    }
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
 * Load dictionary from file.
 * Returns 0 on success, -1 on failure.
 */
static int load_dictionary(FILE* fp, decompressor_ctx_t* ctx, uint64_t dict_offset) {
    /* Seek to dictionary */
    if (fseek(fp, dict_offset, SEEK_SET) != 0) {
        fprintf(stderr, "Error: Failed to seek to dictionary: %s\n", strerror(errno));
        return -1;
    }

    /* Read dictionary header */
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
            case ARG_TYPE_STRING_WITH_LEN:
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

            case ARG_TYPE_STRING:
            case ARG_TYPE_STRING_WITH_LEN: {
                /* Both types serialize identically: [length][data] */
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
                case ARG_TYPE_STRING:
                case ARG_TYPE_STRING_WITH_LEN: {
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
 * Main Decompression
 * ============================================================================ */

static int decompress_file(const char* input_path, FILE* output_fp) {
    FILE* input_fp = NULL;
    decompressor_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
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

        /* Output formatted log line */
        fprintf(output_fp, "[%s] [%s] [%s:%u] %s\n",
                timestamp_str,
                level_to_string(dict->log_level),
                dict->filename,
                dict->line_number,
                message);

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

    if (input_fp != NULL) {
        fclose(input_fp);
    }

    return ret;
}

/* ============================================================================
 * Main Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <input.clog> [output.txt]\n", argv[0]);
        fprintf(stderr, "  If output file not specified, writes to stdout\n");
        return 1;
    }

    const char* input_path = argv[1];
    FILE* output_fp = stdout;

    /* Open output file if specified */
    if (argc == 3) {
        output_fp = fopen(argv[2], "w");
        if (output_fp == NULL) {
            fprintf(stderr, "Error: Cannot open output file '%s': %s\n",
                    argv[2], strerror(errno));
            return 1;
        }
    }

    /* Decompress */
    int ret = decompress_file(input_path, output_fp);

    /* Close output file */
    if (output_fp != stdout) {
        fclose(output_fp);
    }

    return ret;
}
