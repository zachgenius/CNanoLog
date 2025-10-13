/* Copyright (c) 2025
 * CNanoLog Binary Writer Implementation
 */

#include "binary_writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct binary_writer {
    FILE* fp;                   /* File handle */
    char* buffer;               /* Write buffer */
    size_t buffer_size;         /* Size of buffer */
    size_t buffer_used;         /* Bytes used in buffer */
    uint32_t entries_written;   /* Number of log entries written */
    uint64_t bytes_written;     /* Total bytes written to disk */
    uint64_t header_offset;     /* File offset of header (always 0) */
};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * Write data to the buffer, flushing if necessary.
 * Returns 0 on success, -1 on failure.
 */
static int buffer_write(binary_writer_t* writer, const void* data, size_t len) {
    if (writer == NULL || data == NULL) {
        return -1;
    }

    /* If data is larger than buffer, write directly */
    if (len > writer->buffer_size) {
        /* Flush existing buffer first */
        if (writer->buffer_used > 0) {
            if (binwriter_flush(writer) != 0) {
                return -1;
            }
        }

        /* Write large data directly */
        size_t written = fwrite(data, 1, len, writer->fp);
        if (written != len) {
            fprintf(stderr, "binwriter: fwrite failed: %s\n", strerror(errno));
            return -1;
        }
        writer->bytes_written += written;
        return 0;
    }

    /* If data doesn't fit in buffer, flush first */
    if (writer->buffer_used + len > writer->buffer_size) {
        if (binwriter_flush(writer) != 0) {
            return -1;
        }
    }

    /* Copy to buffer */
    memcpy(writer->buffer + writer->buffer_used, data, len);
    writer->buffer_used += len;

    return 0;
}

/**
 * Write dictionary entry (fixed part + variable strings).
 * Returns 0 on success, -1 on failure.
 */
static int write_dict_entry(binary_writer_t* writer, const log_site_info_t* site) {
    cnanolog_dict_entry_t entry;

    /* Fill fixed part */
    entry.log_id = site->log_id;
    entry.log_level = (uint8_t)site->log_level;
    entry.num_args = site->num_args;
    entry.filename_length = (uint16_t)strlen(site->filename);
    entry.format_length = (uint16_t)strlen(site->format);
    entry.line_number = site->line_number;

    /* Copy argument types */
    memset(entry.arg_types, 0, sizeof(entry.arg_types));
    for (int i = 0; i < site->num_args && i < CNANOLOG_MAX_ARGS; i++) {
        entry.arg_types[i] = (uint8_t)site->arg_types[i];
    }

    /* Write fixed part */
    if (buffer_write(writer, &entry, sizeof(entry)) != 0) {
        return -1;
    }

    /* Write filename */
    if (buffer_write(writer, site->filename, entry.filename_length) != 0) {
        return -1;
    }

    /* Write format string */
    if (buffer_write(writer, site->format, entry.format_length) != 0) {
        return -1;
    }

    return 0;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

binary_writer_t* binwriter_create(const char* path) {
    if (path == NULL) {
        fprintf(stderr, "binwriter_create: path is NULL\n");
        return NULL;
    }

    /* Allocate writer structure */
    binary_writer_t* writer = (binary_writer_t*)malloc(sizeof(binary_writer_t));
    if (writer == NULL) {
        fprintf(stderr, "binwriter_create: malloc failed\n");
        return NULL;
    }

    /* Allocate write buffer */
    writer->buffer = (char*)malloc(BINARY_WRITER_BUFFER_SIZE);
    if (writer->buffer == NULL) {
        fprintf(stderr, "binwriter_create: buffer malloc failed\n");
        free(writer);
        return NULL;
    }

    /* Open file for reading and writing (truncate if exists) */
    writer->fp = fopen(path, "w+b");
    if (writer->fp == NULL) {
        fprintf(stderr, "binwriter_create: fopen failed: %s\n", strerror(errno));
        free(writer->buffer);
        free(writer);
        return NULL;
    }

    /* Initialize state */
    writer->buffer_size = BINARY_WRITER_BUFFER_SIZE;
    writer->buffer_used = 0;
    writer->entries_written = 0;
    writer->bytes_written = 0;
    writer->header_offset = 0;

    return writer;
}

int binwriter_write_header(binary_writer_t* writer,
                            uint64_t timestamp_frequency,
                            uint64_t start_timestamp,
                            time_t start_time_sec,
                            int32_t start_time_nsec) {
    if (writer == NULL) {
        return -1;
    }

    /* Build file header */
    cnanolog_file_header_t header;
    memset(&header, 0, sizeof(header));

    header.magic = CNANOLOG_MAGIC;
    header.version_major = CNANOLOG_VERSION_MAJOR;
    header.version_minor = CNANOLOG_VERSION_MINOR;
    header.timestamp_frequency = timestamp_frequency;
    header.start_timestamp = start_timestamp;
    header.start_time_sec = start_time_sec;
    header.start_time_nsec = start_time_nsec;
    header.endianness = CNANOLOG_ENDIAN_MAGIC;
    header.dictionary_offset = 0;  /* Will be updated in close */
    header.entry_count = 0;        /* Will be updated in close */

    /* Write header directly (bypass buffer for alignment) */
    size_t written = fwrite(&header, 1, sizeof(header), writer->fp);
    if (written != sizeof(header)) {
        fprintf(stderr, "binwriter_write_header: fwrite failed: %s\n", strerror(errno));
        return -1;
    }

    writer->bytes_written += written;
    return 0;
}

int binwriter_write_entry(binary_writer_t* writer,
                           uint32_t log_id,
                           uint64_t timestamp,
                           const void* arg_data,
                           uint16_t data_len) {
    if (writer == NULL) {
        return -1;
    }

    /* Validate data length */
    if (data_len > CNANOLOG_MAX_ENTRY_SIZE) {
        fprintf(stderr, "binwriter_write_entry: data too large (%u bytes)\n", data_len);
        return -1;
    }

    /* Build entry header */
    cnanolog_entry_header_t entry_header;
    entry_header.log_id = log_id;
    entry_header.timestamp = timestamp;
    entry_header.data_length = data_len;

    /* Write entry header */
    if (buffer_write(writer, &entry_header, sizeof(entry_header)) != 0) {
        return -1;
    }

    /* Write argument data if present */
    if (data_len > 0 && arg_data != NULL) {
        if (buffer_write(writer, arg_data, data_len) != 0) {
            return -1;
        }
    }

    writer->entries_written++;
    return 0;
}

int binwriter_flush(binary_writer_t* writer) {
    if (writer == NULL) {
        return -1;
    }

    if (writer->buffer_used == 0) {
        return 0;  /* Nothing to flush */
    }

    /* Write buffer to file */
    size_t written = fwrite(writer->buffer, 1, writer->buffer_used, writer->fp);
    if (written != writer->buffer_used) {
        fprintf(stderr, "binwriter_flush: fwrite failed: %s\n", strerror(errno));
        return -1;
    }

    writer->bytes_written += written;
    writer->buffer_used = 0;

    /* Flush to OS */
    if (fflush(writer->fp) != 0) {
        fprintf(stderr, "binwriter_flush: fflush failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int binwriter_close(binary_writer_t* writer,
                    const log_site_info_t* sites,
                    uint32_t num_sites) {
    if (writer == NULL) {
        return -1;
    }

    /* Flush any remaining data */
    if (binwriter_flush(writer) != 0) {
        goto cleanup_error;
    }

    /* Remember dictionary offset (current file position) */
    long dict_offset = ftell(writer->fp);
    if (dict_offset < 0) {
        fprintf(stderr, "binwriter_close: ftell failed: %s\n", strerror(errno));
        goto cleanup_error;
    }

    /* Write dictionary header */
    cnanolog_dict_header_t dict_header;
    dict_header.magic = CNANOLOG_DICT_MAGIC;
    dict_header.num_entries = num_sites;
    dict_header.total_size = sizeof(dict_header);
    dict_header.reserved = 0;

    /* Calculate total dictionary size */
    for (uint32_t i = 0; i < num_sites; i++) {
        dict_header.total_size += sizeof(cnanolog_dict_entry_t);
        dict_header.total_size += (uint32_t)strlen(sites[i].filename);
        dict_header.total_size += (uint32_t)strlen(sites[i].format);
    }

    if (buffer_write(writer, &dict_header, sizeof(dict_header)) != 0) {
        goto cleanup_error;
    }

    /* Write each dictionary entry */
    for (uint32_t i = 0; i < num_sites; i++) {
        if (write_dict_entry(writer, &sites[i]) != 0) {
            goto cleanup_error;
        }
    }

    /* Flush dictionary */
    if (binwriter_flush(writer) != 0) {
        goto cleanup_error;
    }

    /* Seek back to header to update entry_count and dictionary_offset */
    if (fseek(writer->fp, 0, SEEK_SET) != 0) {
        fprintf(stderr, "binwriter_close: fseek failed: %s\n", strerror(errno));
        goto cleanup_error;
    }

    /* Read existing header */
    cnanolog_file_header_t header;
    if (fread(&header, 1, sizeof(header), writer->fp) != sizeof(header)) {
        fprintf(stderr, "binwriter_close: fread header failed: %s\n", strerror(errno));
        goto cleanup_error;
    }

    /* Update fields */
    header.dictionary_offset = (uint64_t)dict_offset;
    header.entry_count = writer->entries_written;

    /* Seek back to beginning and write updated header */
    if (fseek(writer->fp, 0, SEEK_SET) != 0) {
        fprintf(stderr, "binwriter_close: fseek to start failed: %s\n", strerror(errno));
        goto cleanup_error;
    }

    if (fwrite(&header, 1, sizeof(header), writer->fp) != sizeof(header)) {
        fprintf(stderr, "binwriter_close: fwrite header failed: %s\n", strerror(errno));
        goto cleanup_error;
    }

    /* Final flush and close */
    fflush(writer->fp);
    fclose(writer->fp);
    free(writer->buffer);
    free(writer);

    return 0;

cleanup_error:
    /* Clean up on error */
    if (writer->fp != NULL) {
        fclose(writer->fp);
    }
    if (writer->buffer != NULL) {
        free(writer->buffer);
    }
    free(writer);
    return -1;
}

/* ============================================================================
 * Statistics Functions
 * ============================================================================ */

uint32_t binwriter_get_entry_count(const binary_writer_t* writer) {
    if (writer == NULL) {
        return 0;
    }
    return writer->entries_written;
}

uint64_t binwriter_get_bytes_written(const binary_writer_t* writer) {
    if (writer == NULL) {
        return 0;
    }
    return writer->bytes_written;
}

size_t binwriter_get_buffered_bytes(const binary_writer_t* writer) {
    if (writer == NULL) {
        return 0;
    }
    return writer->buffer_used;
}
