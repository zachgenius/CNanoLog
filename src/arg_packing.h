/* Copyright (c) 2025
 * CNanoLog Argument Packing Helpers
 *
 * Pack variadic arguments into binary format for logging.
 */

#pragma once

#include "../include/cnanolog_format.h"
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif


/* ============================================================================
 * Argument Packing
 * ============================================================================ */

/**
 * Pack arguments into binary buffer in a single pass.
 * Returns number of bytes written, or 0 if buffer too small.
 */
static inline size_t arg_pack_write_fast(char* buffer, size_t buffer_size,
                                          uint8_t num_args, const uint8_t* arg_types,
                                          va_list args) {
    char* write_ptr = buffer;
    char* buffer_end = buffer + buffer_size;

    for (uint8_t i = 0; i < num_args; i++) {
        switch (arg_types[i]) {
            case ARG_TYPE_INT32: {
                int32_t val = va_arg(args, int32_t);
                if (write_ptr + sizeof(val) > buffer_end) return 0;
                memcpy(write_ptr, &val, sizeof(val));
                write_ptr += sizeof(val);
                break;
            }
            case ARG_TYPE_INT64: {
                int64_t val = va_arg(args, int64_t);
                if (write_ptr + sizeof(val) > buffer_end) return 0;
                memcpy(write_ptr, &val, sizeof(val));
                write_ptr += sizeof(val);
                break;
            }
            case ARG_TYPE_UINT32: {
                uint32_t val = va_arg(args, uint32_t);
                if (write_ptr + sizeof(val) > buffer_end) return 0;
                memcpy(write_ptr, &val, sizeof(val));
                write_ptr += sizeof(val);
                break;
            }
            case ARG_TYPE_UINT64: {
                uint64_t val = va_arg(args, uint64_t);
                if (write_ptr + sizeof(val) > buffer_end) return 0;
                memcpy(write_ptr, &val, sizeof(val));
                write_ptr += sizeof(val);
                break;
            }
            case ARG_TYPE_DOUBLE: {
                double val = va_arg(args, double);
                if (write_ptr + sizeof(val) > buffer_end) return 0;
                memcpy(write_ptr, &val, sizeof(val));
                write_ptr += sizeof(val);
                break;
            }
            case ARG_TYPE_STRING: {
                const char* str = va_arg(args, const char*);
                uint32_t len = str ? (uint32_t)strlen(str) : 0;
                if (write_ptr + sizeof(len) + len > buffer_end) return 0;
                memcpy(write_ptr, &len, sizeof(len));
                write_ptr += sizeof(len);
                if (len > 0) {
                    memcpy(write_ptr, str, len);
                    write_ptr += len;
                }
                break;
            }
            case ARG_TYPE_STRING_WITH_LEN: {
                const char* str = va_arg(args, const char*);
                uint32_t len = va_arg(args, uint32_t);  /* Pre-calculated, no strlen! */
                if (write_ptr + sizeof(len) + len > buffer_end) return 0;
                memcpy(write_ptr, &len, sizeof(len));
                write_ptr += sizeof(len);
                if (len > 0 && str != NULL) {
                    memcpy(write_ptr, str, len);
                    write_ptr += len;
                }
                break;
            }
            case ARG_TYPE_POINTER: {
                void* ptr = va_arg(args, void*);
                uint64_t val = (uint64_t)ptr;
                if (write_ptr + sizeof(val) > buffer_end) return 0;
                memcpy(write_ptr, &val, sizeof(val));
                write_ptr += sizeof(val);
                break;
            }
            default:
                break;
        }
    }

    return (size_t)(write_ptr - buffer);
}

#ifdef __cplusplus
}
#endif
