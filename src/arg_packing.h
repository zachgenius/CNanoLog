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
 * Argument Size Calculation
 * ============================================================================ */

/**
 * Calculate the size needed to pack arguments.
 */
static inline size_t arg_pack_calc_size(uint8_t num_args, const uint8_t* arg_types, va_list args) {
    size_t size = 0;
    va_list args_copy;
    va_copy(args_copy, args);

    for (uint8_t i = 0; i < num_args; i++) {
        switch (arg_types[i]) {
            case ARG_TYPE_INT32:
                (void)va_arg(args_copy, int32_t);
                size += sizeof(int32_t);
                break;
            case ARG_TYPE_INT64:
                (void)va_arg(args_copy, int64_t);
                size += sizeof(int64_t);
                break;
            case ARG_TYPE_UINT32:
                (void)va_arg(args_copy, uint32_t);
                size += sizeof(uint32_t);
                break;
            case ARG_TYPE_UINT64:
                (void)va_arg(args_copy, uint64_t);
                size += sizeof(uint64_t);
                break;
            case ARG_TYPE_DOUBLE:
                (void)va_arg(args_copy, double);
                size += sizeof(double);
                break;
            case ARG_TYPE_STRING: {
                const char* str = va_arg(args_copy, const char*);
                uint32_t len = str ? (uint32_t)strlen(str) : 0;
                size += sizeof(uint32_t) + len;
                break;
            }
            case ARG_TYPE_POINTER:
                (void)va_arg(args_copy, void*);
                size += sizeof(uint64_t);
                break;
            default:
                break;
        }
    }

    va_end(args_copy);
    return size;
}

/* ============================================================================
 * Argument Packing
 * ============================================================================ */

/**
 * Pack arguments into binary buffer.
 * Returns number of bytes written.
 */
static inline size_t arg_pack_write(char* buffer, size_t buffer_size,
                                     uint8_t num_args, const uint8_t* arg_types,
                                     va_list args) {
    char* write_ptr = buffer;
    char* buffer_end = buffer + buffer_size;

    for (uint8_t i = 0; i < num_args; i++) {
        switch (arg_types[i]) {
            case ARG_TYPE_INT32: {
                int32_t val = va_arg(args, int32_t);
                if (write_ptr + sizeof(val) <= buffer_end) {
                    memcpy(write_ptr, &val, sizeof(val));
                    write_ptr += sizeof(val);
                }
                break;
            }
            case ARG_TYPE_INT64: {
                int64_t val = va_arg(args, int64_t);
                if (write_ptr + sizeof(val) <= buffer_end) {
                    memcpy(write_ptr, &val, sizeof(val));
                    write_ptr += sizeof(val);
                }
                break;
            }
            case ARG_TYPE_UINT32: {
                uint32_t val = va_arg(args, uint32_t);
                if (write_ptr + sizeof(val) <= buffer_end) {
                    memcpy(write_ptr, &val, sizeof(val));
                    write_ptr += sizeof(val);
                }
                break;
            }
            case ARG_TYPE_UINT64: {
                uint64_t val = va_arg(args, uint64_t);
                if (write_ptr + sizeof(val) <= buffer_end) {
                    memcpy(write_ptr, &val, sizeof(val));
                    write_ptr += sizeof(val);
                }
                break;
            }
            case ARG_TYPE_DOUBLE: {
                double val = va_arg(args, double);
                if (write_ptr + sizeof(val) <= buffer_end) {
                    memcpy(write_ptr, &val, sizeof(val));
                    write_ptr += sizeof(val);
                }
                break;
            }
            case ARG_TYPE_STRING: {
                const char* str = va_arg(args, const char*);
                uint32_t len = str ? (uint32_t)strlen(str) : 0;
                if (write_ptr + sizeof(len) + len <= buffer_end) {
                    memcpy(write_ptr, &len, sizeof(len));
                    write_ptr += sizeof(len);
                    if (len > 0) {
                        memcpy(write_ptr, str, len);
                        write_ptr += len;
                    }
                }
                break;
            }
            case ARG_TYPE_POINTER: {
                void* ptr = va_arg(args, void*);
                uint64_t val = (uint64_t)ptr;
                if (write_ptr + sizeof(val) <= buffer_end) {
                    memcpy(write_ptr, &val, sizeof(val));
                    write_ptr += sizeof(val);
                }
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

#endif /* CNANOLOG_ARG_PACKING_H */
