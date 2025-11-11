/* Copyright (c) 2025
 * CNanoLog Text Formatter Implementation
 *
 * Formats binary log entries to human-readable text in background thread.
 */

#include "text_formatter.h"
#include "../include/cnanolog.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Buffer size for message formatting */
#define MESSAGE_BUFFER_SIZE 8192

/* ============================================================================
 * Text Writer Implementation
 * ============================================================================ */

struct text_writer {
    FILE* file;
    uint64_t timestamp_frequency;
    uint64_t start_timestamp;
    time_t start_time_sec;
    int32_t start_time_nsec;
    uint64_t bytes_written;  /* Track total bytes written for statistics */
    const char* pattern;     /* Format pattern (NULL = use default) */
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * Convert log level enum to string.
 */
static const char* level_to_string(cnanolog_level_t level) {
    switch (level) {
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_DEBUG: return "DEBUG";
        default: {
            static char buf[16];
            snprintf(buf, sizeof(buf), "LEVEL_%u", (unsigned int)level);
            return buf;
        }
    }
}

/**
 * Format rdtsc timestamp to human-readable string.
 * Output: YYYY-MM-DD HH:MM:SS.nnnnnnnnn
 */
static void format_timestamp(text_writer_t* writer, uint64_t timestamp,
                              char* buf, size_t buf_size) {
#ifndef CNANOLOG_NO_TIMESTAMPS
    if (writer->timestamp_frequency == 0) {
        snprintf(buf, buf_size, "NO_TIMESTAMP");
        return;
    }

    /* Calculate elapsed time since start */
    uint64_t elapsed_ticks = timestamp - writer->start_timestamp;
    double elapsed_seconds = (double)elapsed_ticks / (double)writer->timestamp_frequency;

    /* Calculate wall-clock time */
    time_t wall_time = writer->start_time_sec + (time_t)elapsed_seconds;
    double fractional = elapsed_seconds - (time_t)elapsed_seconds;
    long nanoseconds = writer->start_time_nsec + (long)(fractional * 1000000000.0);

    /* Handle nanosecond overflow */
    if (nanoseconds >= 1000000000L) {
        wall_time += 1;
        nanoseconds -= 1000000000L;
    } else if (nanoseconds < 0) {
        wall_time -= 1;
        nanoseconds += 1000000000L;
    }

    /* Format to string */
    struct tm tm_buf;
    struct tm* tm = localtime_r(&wall_time, &tm_buf);
    if (tm != NULL) {
        snprintf(buf, buf_size, "%04d-%02d-%02d %02d:%02d:%02d.%09ld",
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                 tm->tm_hour, tm->tm_min, tm->tm_sec,
                 nanoseconds);
    } else {
        snprintf(buf, buf_size, "INVALID_TIME");
    }
#else
    (void)writer;
    (void)timestamp;
    snprintf(buf, buf_size, "NO_TIMESTAMP");
#endif
}

/**
 * Data from staging buffers is already uncompressed.
 * Just return it as-is. (Compression only happens for binary mode)
 */
static const char* get_uncompressed_data(text_writer_t* writer,
                                         const char* arg_data,
                                         uint16_t arg_data_len,
                                         const log_site_t* site,
                                         size_t* uncompressed_len) {
    (void)writer;  /* Unused */
    (void)site;    /* Unused */

    /* Data from staging buffer is always uncompressed */
    *uncompressed_len = arg_data_len;
    return arg_data;
}

/**
 * Format message by substituting arguments into format string.
 * Reads arguments from binary data and formats them.
 */
static void format_message(const log_site_t* site,
                           const char* arg_data,
                           char* output,
                           size_t output_size) {
    const char* read_ptr = arg_data;
    char* write_ptr = output;
    const char* fmt_ptr = site->format;
    const char* output_end = output + output_size - 1;
    int arg_index = 0;

    /* Process format string */
    while (*fmt_ptr && write_ptr < output_end) {
        if (*fmt_ptr == '%' && *(fmt_ptr + 1) != '%') {
            /* Found format specifier */
            if (arg_index >= site->num_args) {
                /* No more arguments - just copy the % */
                *write_ptr++ = *fmt_ptr++;
                continue;
            }

            cnanolog_arg_type_t arg_type = (cnanolog_arg_type_t)site->arg_types[arg_index];
            arg_index++;

            /* Skip the % and format specifier */
            fmt_ptr++;
            while (*fmt_ptr && strchr("-+ #0123456789.*lhz", *fmt_ptr)) {
                fmt_ptr++;
            }
            if (*fmt_ptr) fmt_ptr++;  /* Skip conversion specifier */

            /* Extract and format argument */
            int remaining = output_end - write_ptr;
            switch (arg_type) {
                case ARG_TYPE_CHAR: {
                    char val;
                    memcpy(&val, read_ptr, sizeof(val));
                    read_ptr += sizeof(val);
                    int written = snprintf(write_ptr, remaining, "%c", (int)val);
                    write_ptr += (written > 0 && written < remaining) ? written : 0;
                    break;
                }

                case ARG_TYPE_INT32: {
                    int32_t val;
                    memcpy(&val, read_ptr, sizeof(val));
                    read_ptr += sizeof(val);
                    int written = snprintf(write_ptr, remaining, "%d", val);
                    write_ptr += (written > 0 && written < remaining) ? written : 0;
                    break;
                }

                case ARG_TYPE_INT64: {
                    int64_t val;
                    memcpy(&val, read_ptr, sizeof(val));
                    read_ptr += sizeof(val);
                    int written = snprintf(write_ptr, remaining, "%lld", (long long)val);
                    write_ptr += (written > 0 && written < remaining) ? written : 0;
                    break;
                }

                case ARG_TYPE_UINT32: {
                    uint32_t val;
                    memcpy(&val, read_ptr, sizeof(val));
                    read_ptr += sizeof(val);
                    int written = snprintf(write_ptr, remaining, "%u", val);
                    write_ptr += (written > 0 && written < remaining) ? written : 0;
                    break;
                }

                case ARG_TYPE_UINT64: {
                    uint64_t val;
                    memcpy(&val, read_ptr, sizeof(val));
                    read_ptr += sizeof(val);
                    int written = snprintf(write_ptr, remaining, "%llu", (unsigned long long)val);
                    write_ptr += (written > 0 && written < remaining) ? written : 0;
                    break;
                }

                case ARG_TYPE_DOUBLE: {
                    double val;
                    memcpy(&val, read_ptr, sizeof(val));
                    read_ptr += sizeof(val);
                    int written = snprintf(write_ptr, remaining, "%f", val);
                    write_ptr += (written > 0 && written < remaining) ? written : 0;
                    break;
                }

                case ARG_TYPE_STRING: {
                    uint32_t str_len;
                    memcpy(&str_len, read_ptr, sizeof(str_len));
                    read_ptr += sizeof(str_len);

                    /* Copy string directly (not null-terminated in binary) */
                    size_t copy_len = str_len;
                    if (write_ptr + copy_len >= output_end) {
                        copy_len = output_end - write_ptr;
                    }
                    if (copy_len > 0) {
                        memcpy(write_ptr, read_ptr, copy_len);
                        write_ptr += copy_len;
                    }
                    read_ptr += str_len;
                    break;
                }

                case ARG_TYPE_POINTER: {
                    uint64_t val;
                    memcpy(&val, read_ptr, sizeof(val));
                    read_ptr += sizeof(val);
                    int written = snprintf(write_ptr, remaining, "%p", (void*)val);
                    write_ptr += (written > 0 && written < remaining) ? written : 0;
                    break;
                }

                default:
                    break;
            }
        } else {
            /* Regular character or %% */
            *write_ptr++ = *fmt_ptr++;
            if (*(fmt_ptr - 1) == '%' && *fmt_ptr == '%') {
                fmt_ptr++;  /* Skip second % */
            }
        }
    }

    *write_ptr = '\0';
}

/**
 * Format entry according to custom pattern.
 * Replaces pattern tokens with actual values.
 */
static void format_entry_with_pattern(
    const char* pattern,
    const char* timestamp_buf,
    const char* level_str,
    const log_site_t* site,
    const char* message_buf,
    char* output,
    size_t output_size)
{
    const char* p = pattern;
    char* out = output;
    char* out_end = output + output_size - 1;

    while (*p && out < out_end) {
        if (*p == '%' && *(p + 1)) {
            p++;  /* Skip % */
            switch (*p) {
                case 't':  /* Full timestamp: YYYY-MM-DD HH:MM:SS.nnnnnnnnn */
                    out += snprintf(out, out_end - out, "%s", timestamp_buf);
                    break;

                case 'T': {  /* Short timestamp: HH:MM:SS.nnn (microsecond precision) */
                    /* Extract time portion and first 3 decimal digits */
                    if (strlen(timestamp_buf) >= 23) {
                        /* Format: YYYY-MM-DD HH:MM:SS.nnnnnnnnn */
                        /*         0123456789012345678901234567 */
                        /* Extract: HH:MM:SS.nnn (11-22 range, take first 12 chars) */
                        out += snprintf(out, out_end - out, "%.12s", timestamp_buf + 11);
                    } else {
                        out += snprintf(out, out_end - out, "%s", timestamp_buf);
                    }
                    break;
                }

                case 'd':  /* Date only: YYYY-MM-DD */
                    out += snprintf(out, out_end - out, "%.10s", timestamp_buf);
                    break;

                case 'D': {  /* Time only: HH:MM:SS */
                    if (strlen(timestamp_buf) >= 19) {
                        out += snprintf(out, out_end - out, "%.8s", timestamp_buf + 11);
                    } else {
                        out += snprintf(out, out_end - out, "%s", timestamp_buf);
                    }
                    break;
                }

                case 'l':  /* Log level name: INFO, WARN, ERROR, DEBUG */
                    out += snprintf(out, out_end - out, "%s", level_str);
                    break;

                case 'L':  /* Log level letter: I, W, E, D */
                    out += snprintf(out, out_end - out, "%c", level_str[0]);
                    break;

                case 'f':  /* Filename (basename) */
                    out += snprintf(out, out_end - out, "%s", site->filename);
                    break;

                case 'F':  /* Full file path (same as basename for now) */
                    out += snprintf(out, out_end - out, "%s", site->filename);
                    break;

                case 'n':  /* Line number */
                    out += snprintf(out, out_end - out, "%u", site->line_number);
                    break;

                case 'm':  /* Formatted message */
                    out += snprintf(out, out_end - out, "%s", message_buf);
                    break;

                case '%':  /* Literal % */
                    if (out < out_end) *out++ = '%';
                    break;

                default:  /* Unknown token - output as-is */
                    if (out < out_end) *out++ = '%';
                    if (out < out_end) *out++ = *p;
                    break;
            }
            p++;
        } else {
            /* Regular character - copy as-is */
            *out++ = *p++;
        }
    }

    *out = '\0';
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

text_writer_t* text_writer_create(const char* file_path) {
    if (file_path == NULL) {
        return NULL;
    }

    text_writer_t* writer = (text_writer_t*)calloc(1, sizeof(text_writer_t));
    if (writer == NULL) {
        return NULL;
    }

    /* Open file in append mode */
    writer->file = fopen(file_path, "a");
    if (writer->file == NULL) {
        free(writer);
        return NULL;
    }

    /* Enable line buffering for better tail -f experience */
    setvbuf(writer->file, NULL, _IOLBF, 0);

    writer->bytes_written = 0;
    writer->pattern = NULL;  /* NULL = use default pattern */
    return writer;
}

void text_writer_set_timestamp_info(text_writer_t* writer,
                                     uint64_t frequency,
                                     uint64_t start_timestamp,
                                     time_t start_time_sec,
                                     int32_t start_time_nsec) {
    if (writer == NULL) {
        return;
    }

    writer->timestamp_frequency = frequency;
    writer->start_timestamp = start_timestamp;
    writer->start_time_sec = start_time_sec;
    writer->start_time_nsec = start_time_nsec;
}

void text_writer_set_pattern(text_writer_t* writer, const char* pattern) {
    if (writer == NULL) {
        return;
    }
    writer->pattern = pattern;  /* NULL = use default pattern */
}

int text_writer_write_entry(text_writer_t* writer,
                             uint32_t log_id,
                             uint64_t timestamp,
                             const char* arg_data,
                             uint16_t arg_data_len,
                             const log_registry_t* registry) {
    if (writer == NULL || writer->file == NULL || registry == NULL) {
        return -1;
    }

    /* Lookup log site */
    const log_site_t* site = log_registry_get(registry, log_id);
    if (site == NULL) {
        fprintf(writer->file, "[UNKNOWN_LOG_ID_%u]\n", log_id);
        return -1;
    }

    /* Format timestamp */
    char timestamp_buf[64];
    format_timestamp(writer, timestamp, timestamp_buf, sizeof(timestamp_buf));

    /* Get uncompressed data (already uncompressed from staging buffer) */
    size_t uncompressed_len = 0;
    const char* uncompressed_data = get_uncompressed_data(writer, arg_data, arg_data_len,
                                                           site, &uncompressed_len);

    /* Format message */
    char message_buf[MESSAGE_BUFFER_SIZE];
    format_message(site, uncompressed_data, message_buf, sizeof(message_buf));

    /* Get level string */
    const char* level_str = level_to_string(site->log_level);

    /* Format complete log line using pattern
     * Priority: 1) Per-log pattern, 2) Global pattern, 3) Default pattern */
    char output_buf[MESSAGE_BUFFER_SIZE + 256];  /* Extra space for timestamp, etc. */
    const char* pattern = site->text_pattern ? site->text_pattern :
                          (writer->pattern ? writer->pattern : "[%t] [%l] [%f:%n] %m");

    format_entry_with_pattern(pattern, timestamp_buf, level_str, site,
                              message_buf, output_buf, sizeof(output_buf));

    /* Write formatted line (with newline) */
    int written = fprintf(writer->file, "%s\n", output_buf);

    if (written > 0) {
        writer->bytes_written += written;
    }

    return (written > 0) ? 0 : -1;
}

void text_writer_flush(text_writer_t* writer) {
    if (writer != NULL && writer->file != NULL) {
        fflush(writer->file);
    }
}

int text_writer_rotate(text_writer_t* writer, const char* new_path) {
    if (writer == NULL || new_path == NULL) {
        return -1;
    }

    /* Close current file */
    if (writer->file != NULL) {
        fclose(writer->file);
    }

    /* Open new file */
    writer->file = fopen(new_path, "a");
    if (writer->file == NULL) {
        return -1;
    }

    /* Enable line buffering */
    setvbuf(writer->file, NULL, _IOLBF, 0);

    return 0;
}

void text_writer_close(text_writer_t* writer) {
    if (writer == NULL) {
        return;
    }

    if (writer->file != NULL) {
        fflush(writer->file);
        fclose(writer->file);
        writer->file = NULL;
    }

    free(writer);
}

uint64_t text_writer_get_bytes_written(text_writer_t* writer) {
    return (writer != NULL) ? writer->bytes_written : 0;
}
