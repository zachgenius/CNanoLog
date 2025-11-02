/* Copyright (c) 2025
 * CNanoLog Binary Writer Implementation
 *
 * Uses POSIX AIO (Asynchronous I/O) to eliminate blocking:
 * - aio_write() returns immediately (non-blocking)
 * - Kernel handles I/O in background
 * - Background thread never blocks on I/O
 * - No cache coherency delays → consistent low latency
 */

#include "binary_writer.h"
#include "log_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifndef _WIN32
#include <unistd.h>  /* For fsync(), close() */
#include <fcntl.h>   /* For open() */
#include <aio.h>     /* For POSIX AIO */
#endif

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct binary_writer {
    int fd;                     /* File descriptor (for AIO) */
    FILE* fp;                   /* File handle (for header/dictionary updates) */

    /* Double buffering for async I/O */
    char* buffers[2];           /* Buffer A and Buffer B */
    int active_buffer_idx;      /* Index of currently active buffer (0 or 1) */
    size_t buffer_used;         /* Bytes used in active buffer */
    size_t buffer_size;         /* Size of each buffer */

    /* POSIX AIO state */
    struct aiocb aiocb;         /* AIO control block */
    int has_outstanding_aio;    /* Flag: AIO operation in progress */

    uint32_t entries_written;   /* Number of log entries written */
    uint64_t bytes_written;     /* Total bytes written to disk */
    uint64_t header_offset;     /* File offset of header (always 0) */
};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * Check and wait for any outstanding AIO operation to complete.
 * Returns 0 on success, -1 on failure.
 */
static int wait_for_aio(binary_writer_t* writer) {
#ifdef _WIN32
    return 0;  /* Windows doesn't support POSIX AIO */
#else
    if (!writer->has_outstanding_aio) {
        return 0;  /* No outstanding operation */
    }

    /* Check if still in progress */
    int err = aio_error(&writer->aiocb);
    if (err == EINPROGRESS) {
        /* Wait for completion */
        const struct aiocb* aiocb_list[] = {&writer->aiocb};
        if (aio_suspend(aiocb_list, 1, NULL) != 0) {
            perror("binwriter: aio_suspend failed");
            return -1;
        }
        err = aio_error(&writer->aiocb);
    }

    /* Get result */
    ssize_t ret = aio_return(&writer->aiocb);
    if (err != 0) {
        fprintf(stderr, "binwriter: POSIX AIO failed with %d: %s\n",
                err, strerror(err));
        return -1;
    }
    if (ret < 0) {
        perror("binwriter: AIO write operation failed");
        return -1;
    }

    writer->has_outstanding_aio = 0;
    return 0;
#endif
}

/**
 * Initiate async write of active buffer.
 * Returns 0 on success, -1 on failure.
 */
static int async_flush_buffer(binary_writer_t* writer) {
    if (writer->buffer_used == 0) {
        return 0;  /* Nothing to flush */
    }

#if defined(__linux__)
    /* Wait for any previous AIO to complete before starting new write */
    if (wait_for_aio(writer) != 0) {
        return -1;
    }

    /* Start async write of current buffer */
    memset(&writer->aiocb, 0, sizeof(writer->aiocb));
    writer->aiocb.aio_fildes = writer->fd;
    writer->aiocb.aio_buf = writer->buffers[writer->active_buffer_idx];
    writer->aiocb.aio_nbytes = writer->buffer_used;
    writer->aiocb.aio_offset = writer->bytes_written;

    if (aio_write(&writer->aiocb) == -1) {
        fprintf(stderr, "binwriter: aio_write failed: %s\n", strerror(errno));
        return -1;
    }

    writer->has_outstanding_aio = 1;
    writer->bytes_written += writer->buffer_used;

    /* Swap to other buffer */
    writer->active_buffer_idx = 1 - writer->active_buffer_idx;
    writer->buffer_used = 0;

    return 0;

#else
    /* macOS/Windows: Fallback to synchronous write() */
    ssize_t written = write(writer->fd, writer->buffers[writer->active_buffer_idx],
                           writer->buffer_used);
    if (written != (ssize_t)writer->buffer_used) {
        fprintf(stderr, "binwriter: write failed: %s\n", strerror(errno));
        return -1;
    }

    writer->bytes_written += written;
    writer->buffer_used = 0;
    return 0;
#endif
}

/**
 * Write data to the buffer, flushing if necessary.
 * Returns 0 on success, -1 on failure.
 */
static int buffer_write(binary_writer_t* writer, const void* data, size_t len) {
    if (writer == NULL || data == NULL) {
        return -1;
    }

    /* If data is larger than buffer, flush and write directly */
    if (len > writer->buffer_size) {
        /* Flush existing buffer first */
        if (writer->buffer_used > 0) {
            if (async_flush_buffer(writer) != 0) {
                return -1;
            }
        }

        /* Wait for AIO to complete, then write large data synchronously */
        if (wait_for_aio(writer) != 0) {
            return -1;
        }

        ssize_t written = write(writer->fd, data, len);
        if (written != (ssize_t)len) {
            fprintf(stderr, "binwriter: write failed: %s\n", strerror(errno));
            return -1;
        }
        writer->bytes_written += written;
        return 0;
    }

    /* If data doesn't fit in buffer, flush first */
    if (writer->buffer_used + len > writer->buffer_size) {
        if (async_flush_buffer(writer) != 0) {
            return -1;
        }
    }

    /* Copy to active buffer */
    memcpy(writer->buffers[writer->active_buffer_idx] + writer->buffer_used,
           data, len);
    writer->buffer_used += len;

    return 0;
}

/**
 * Write dictionary entry (fixed part + variable strings).
 * Returns 0 on success, -1 on failure.
 */
static int write_dict_entry(binary_writer_t* writer, const log_site_t* site) {
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
    memset(writer, 0, sizeof(binary_writer_t));

    /* Allocate double buffers for async I/O */
    writer->buffers[0] = (char*)malloc(BINARY_WRITER_BUFFER_SIZE);
    writer->buffers[1] = (char*)malloc(BINARY_WRITER_BUFFER_SIZE);
    if (writer->buffers[0] == NULL || writer->buffers[1] == NULL) {
        fprintf(stderr, "binwriter_create: buffer malloc failed\n");
        free(writer->buffers[0]);
        free(writer->buffers[1]);
        free(writer);
        return NULL;
    }

#ifndef _WIN32
    /* Open file descriptor for AIO */
    writer->fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (writer->fd == -1) {
        fprintf(stderr, "binwriter_create: open failed: %s\n", strerror(errno));
        free(writer->buffers[0]);
        free(writer->buffers[1]);
        free(writer);
        return NULL;
    }
#endif

    /* Open FILE* for header/dictionary updates (random access) */
    writer->fp = fopen(path, "r+b");
    if (writer->fp == NULL) {
        fprintf(stderr, "binwriter_create: fopen failed: %s\n", strerror(errno));
#ifndef _WIN32
        close(writer->fd);
#endif
        free(writer->buffers[0]);
        free(writer->buffers[1]);
        free(writer);
        return NULL;
    }

    /* Initialize state */
    writer->buffer_size = BINARY_WRITER_BUFFER_SIZE;
    writer->buffer_used = 0;
    writer->active_buffer_idx = 0;
    writer->has_outstanding_aio = 0;
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

    /* Set flags based on compile-time configuration */
    header.flags = 0;
#ifndef CNANOLOG_NO_TIMESTAMPS
    header.flags |= CNANOLOG_FLAG_HAS_TIMESTAMPS;
#endif

    /* Write header directly using fd (must be consistent with entry writes) */
    ssize_t written = write(writer->fd, &header, sizeof(header));
    if (written != sizeof(header)) {
        fprintf(stderr, "binwriter_write_header: write failed: %s\n", strerror(errno));
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
#ifndef CNANOLOG_NO_TIMESTAMPS
    entry_header.timestamp = timestamp;
#else
    (void)timestamp;  /* Suppress unused parameter warning */
#endif
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

    return async_flush_buffer(writer);
}

int binwriter_close(binary_writer_t* writer,
                    const log_site_t* sites,
                    uint32_t num_sites) {
    if (writer == NULL) {
        return -1;
    }

    /* Flush any remaining data */
    if (binwriter_flush(writer) != 0) {
        goto cleanup_error;
    }

    /* Wait for all outstanding AIO to complete before dictionary */
    if (wait_for_aio(writer) != 0) {
        goto cleanup_error;
    }

    /* Dictionary starts at current write position */
    uint64_t dict_offset = writer->bytes_written;

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

    /* Wait for final AIO to complete */
    if (wait_for_aio(writer) != 0) {
        goto cleanup_error;
    }

    /* Use FILE* for header update (random access to beginning of file) */
    /* Seek to beginning to read/update header */
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

    /* Final flush and sync to disk for data durability */
    fflush(writer->fp);
#ifndef _WIN32
    /* Ensure all data is written to disk (POSIX) */
    fsync(writer->fd);
    close(writer->fd);
#endif
    fclose(writer->fp);
    free(writer->buffers[0]);
    free(writer->buffers[1]);
    free(writer);

    return 0;

cleanup_error:
    /* Clean up on error */
#ifndef _WIN32
    if (writer->fd >= 0) {
        close(writer->fd);
    }
#endif
    if (writer->fp != NULL) {
        fclose(writer->fp);
    }
    free(writer->buffers[0]);
    free(writer->buffers[1]);
    free(writer);
    return -1;
}

int binwriter_rotate(binary_writer_t* writer,
                     const char* new_path,
                     const log_site_t* sites,
                     uint32_t num_sites,
                     uint64_t timestamp_frequency,
                     uint64_t start_timestamp,
                     time_t start_time_sec,
                     int32_t start_time_nsec) {
    if (writer == NULL || new_path == NULL) {
        return -1;
    }

    /* Step 1: Flush and close current file with dictionary */
    if (binwriter_flush(writer) != 0) {
        fprintf(stderr, "binwriter_rotate: flush failed\n");
        return -1;
    }

    if (wait_for_aio(writer) != 0) {
        fprintf(stderr, "binwriter_rotate: wait for AIO failed\n");
        return -1;
    }

    /* Write dictionary to current file */
    uint64_t dict_offset = writer->bytes_written;

    cnanolog_dict_header_t dict_header;
    dict_header.magic = CNANOLOG_DICT_MAGIC;
    dict_header.num_entries = num_sites;
    dict_header.total_size = sizeof(dict_header);
    dict_header.reserved = 0;

    for (uint32_t i = 0; i < num_sites; i++) {
        dict_header.total_size += sizeof(cnanolog_dict_entry_t);
        dict_header.total_size += (uint32_t)strlen(sites[i].filename);
        dict_header.total_size += (uint32_t)strlen(sites[i].format);
    }

    if (buffer_write(writer, &dict_header, sizeof(dict_header)) != 0) {
        fprintf(stderr, "binwriter_rotate: write dict header failed\n");
        return -1;
    }

    for (uint32_t i = 0; i < num_sites; i++) {
        if (write_dict_entry(writer, &sites[i]) != 0) {
            fprintf(stderr, "binwriter_rotate: write dict entry failed\n");
            return -1;
        }
    }

    if (binwriter_flush(writer) != 0) {
        fprintf(stderr, "binwriter_rotate: flush dict failed\n");
        return -1;
    }

    if (wait_for_aio(writer) != 0) {
        fprintf(stderr, "binwriter_rotate: wait for dict AIO failed\n");
        return -1;
    }

    /* Update header with entry count and dictionary offset */
    if (fseek(writer->fp, 0, SEEK_SET) != 0) {
        fprintf(stderr, "binwriter_rotate: fseek to header failed\n");
        return -1;
    }

    cnanolog_file_header_t header;
    if (fread(&header, 1, sizeof(header), writer->fp) != sizeof(header)) {
        fprintf(stderr, "binwriter_rotate: read header failed\n");
        return -1;
    }

    header.dictionary_offset = dict_offset;
    header.entry_count = writer->entries_written;

    if (fseek(writer->fp, 0, SEEK_SET) != 0) {
        fprintf(stderr, "binwriter_rotate: fseek to start failed\n");
        return -1;
    }

    if (fwrite(&header, 1, sizeof(header), writer->fp) != sizeof(header)) {
        fprintf(stderr, "binwriter_rotate: write header failed\n");
        return -1;
    }

    fflush(writer->fp);
#ifndef _WIN32
    fsync(writer->fd);
    close(writer->fd);
#endif
    fclose(writer->fp);

    /* Step 2: Open new file */
#ifndef _WIN32
    writer->fd = open(new_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (writer->fd == -1) {
        fprintf(stderr, "binwriter_rotate: open new file failed: %s\n", strerror(errno));
        return -1;
    }
#endif

    writer->fp = fopen(new_path, "r+b");
    if (writer->fp == NULL) {
        fprintf(stderr, "binwriter_rotate: fopen new file failed: %s\n", strerror(errno));
#ifndef _WIN32
        close(writer->fd);
#endif
        return -1;
    }

    /* Reset writer state for new file */
    writer->buffer_used = 0;
    writer->active_buffer_idx = 0;
    writer->has_outstanding_aio = 0;
    writer->entries_written = 0;
    writer->bytes_written = 0;

    /* Step 3: Write new file header */
    cnanolog_file_header_t new_header;
    memset(&new_header, 0, sizeof(new_header));

    new_header.magic = CNANOLOG_MAGIC;
    new_header.version_major = CNANOLOG_VERSION_MAJOR;
    new_header.version_minor = CNANOLOG_VERSION_MINOR;
    new_header.timestamp_frequency = timestamp_frequency;
    new_header.start_timestamp = start_timestamp;
    new_header.start_time_sec = start_time_sec;
    new_header.start_time_nsec = start_time_nsec;
    new_header.endianness = CNANOLOG_ENDIAN_MAGIC;
    new_header.dictionary_offset = 0;
    new_header.entry_count = 0;

    new_header.flags = 0;
#ifndef CNANOLOG_NO_TIMESTAMPS
    new_header.flags |= CNANOLOG_FLAG_HAS_TIMESTAMPS;
#endif

    ssize_t written = write(writer->fd, &new_header, sizeof(new_header));
    if (written != sizeof(new_header)) {
        fprintf(stderr, "binwriter_rotate: write new header failed\n");
        return -1;
    }

    writer->bytes_written += written;
    return 0;
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
