/* Copyright (c) 2025
 * CNanoLog Type Detection Macros
 *
 * Uses C11 _Generic or C++ templates to automatically detect argument types.
 */

#pragma once

#include "cnanolog_format.h"
#include <stdint.h>

/* ============================================================================
 * Type Detection - C11 _Generic vs C++ Templates
 * ============================================================================ */

#ifdef __cplusplus
}  /* Temporarily close extern "C" from parent headers */

    /* C++ - use template metaprogramming for type detection */
    #define CNANOLOG_HAS_GENERIC 0

    #include <type_traits>

    /* Template-based type detection using type traits */
    namespace cnanolog_detail {
        template<typename T>
        struct TypeDetector {
            static constexpr uint8_t value() {
                using U = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

                // String types
                if (std::is_same<U, char*>::value || std::is_same<U, const char*>::value ||
                    std::is_same<U, char* const>::value) {
                    return ARG_TYPE_STRING;
                }

                // Floating point
                if (std::is_floating_point<U>::value) {
                    return ARG_TYPE_DOUBLE;
                }

                // Signed integers
                if (std::is_integral<U>::value && std::is_signed<U>::value) {
                    return sizeof(U) <= 4 ? ARG_TYPE_INT32 : ARG_TYPE_INT64;
                }

                // Unsigned integers
                if (std::is_integral<U>::value && std::is_unsigned<U>::value) {
                    return sizeof(U) <= 4 ? ARG_TYPE_UINT32 : ARG_TYPE_UINT64;
                }

                // Pointers
                if (std::is_pointer<U>::value) {
                    return ARG_TYPE_POINTER;
                }

                // Default to pointer
                return ARG_TYPE_POINTER;
            }
        };
    }

    #define CNANOLOG_ARG_TYPE(x) (::cnanolog_detail::TypeDetector<decltype(x)>::value())

extern "C" {  /* Re-open extern "C" for remaining content */
#else
    /* C11 - use _Generic for automatic type detection */
    #if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
        #define CNANOLOG_HAS_GENERIC 1

        /**
         * Detect the type of a single argument.
         * Maps C types to cnanolog_arg_type_t enum values.
         *
         * Note: Avoid duplicate types in _Generic. On most platforms:
         *   - int == int32_t == signed int
         *   - long long == int64_t
         *   - unsigned int == uint32_t
         *   - unsigned long long == uint64_t
         */
        #define CNANOLOG_ARG_TYPE(x) _Generic((x), \
            int:                ARG_TYPE_INT32, \
            short:              ARG_TYPE_INT32, \
            char:               ARG_TYPE_INT32, \
            signed char:        ARG_TYPE_INT32, \
            \
            long:               ARG_TYPE_INT64, \
            long long:          ARG_TYPE_INT64, \
            \
            unsigned int:       ARG_TYPE_UINT32, \
            unsigned short:     ARG_TYPE_UINT32, \
            unsigned char:      ARG_TYPE_UINT32, \
            \
            unsigned long:      ARG_TYPE_UINT64, \
            unsigned long long: ARG_TYPE_UINT64, \
            \
            float:              ARG_TYPE_DOUBLE, \
            double:             ARG_TYPE_DOUBLE, \
            \
            char*:              ARG_TYPE_STRING, \
            const char*:        ARG_TYPE_STRING, \
            \
            default:            ARG_TYPE_POINTER)
    #else
        #define CNANOLOG_HAS_GENERIC 0
        #error "CNanoLog requires C11 or later for automatic type detection"
    #endif
#endif

/* ============================================================================
 * Argument Counting
 * ============================================================================ */

/**
 * Count the number of arguments at compile time.
 * Uses a common preprocessor trick.
 *
 * Note: Handling zero arguments is tricky. We use a sentinel approach.
 */
#define CNANOLOG_COUNT_ARGS(...) \
    CNANOLOG_COUNT_ARGS_IMPL(0, ##__VA_ARGS__, \
        16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define CNANOLOG_COUNT_ARGS_IMPL( \
    _0, _1, _2, _3, _4, _5, _6, _7, _8, \
    _9, _10, _11, _12, _13, _14, _15, _16, N, ...) N

/* ============================================================================
 * Type Array Builder
 * ============================================================================ */

/**
 * Build array of argument types.
 * Expands to (uint8_t[]){type1, type2, ...}
 *
 * We need variadic macros that handle 0-16 arguments.
 */

/* Helper: Get Nth argument or 0 if not present */
#define CNANOLOG_GET_ARG_1(_1, ...) _1
#define CNANOLOG_GET_ARG_2(_1, _2, ...) _2
#define CNANOLOG_GET_ARG_3(_1, _2, _3, ...) _3
#define CNANOLOG_GET_ARG_4(_1, _2, _3, _4, ...) _4
#define CNANOLOG_GET_ARG_5(_1, _2, _3, _4, _5, ...) _5
#define CNANOLOG_GET_ARG_6(_1, _2, _3, _4, _5, _6, ...) _6
#define CNANOLOG_GET_ARG_7(_1, _2, _3, _4, _5, _6, _7, ...) _7
#define CNANOLOG_GET_ARG_8(_1, _2, _3, _4, _5, _6, _7, _8, ...) _8
#define CNANOLOG_GET_ARG_9(_1, _2, _3, _4, _5, _6, _7, _8, _9, ...) _9
#define CNANOLOG_GET_ARG_10(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, ...) _10
#define CNANOLOG_GET_ARG_11(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, ...) _11
#define CNANOLOG_GET_ARG_12(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, ...) _12
#define CNANOLOG_GET_ARG_13(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, ...) _13
#define CNANOLOG_GET_ARG_14(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, ...) _14
#define CNANOLOG_GET_ARG_15(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, ...) _15
#define CNANOLOG_GET_ARG_16(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, ...) _16

/**
 * Build type array for variable number of arguments.
 * Handles 0-16 arguments gracefully.
 *
 * Uses extra level of indirection to force macro expansion of count.
 */
#define CNANOLOG_ARG_TYPES(...) \
    CNANOLOG_ARG_TYPES_IMPL(CNANOLOG_COUNT_ARGS(__VA_ARGS__), ##__VA_ARGS__)

#define CNANOLOG_ARG_TYPES_IMPL(count, ...) \
    CNANOLOG_ARG_TYPES_IMPL2(count, ##__VA_ARGS__)

#define CNANOLOG_ARG_TYPES_IMPL2(count, ...) \
    CNANOLOG_ARG_TYPES_##count(__VA_ARGS__)

/* Specializations for each argument count
 * Returns brace initializer (without cast) that works for both C11 and C++ array initialization */
#define CNANOLOG_ARG_TYPES_0() \
    {0}

#define CNANOLOG_ARG_TYPES_1(_1) \
    {CNANOLOG_ARG_TYPE(_1)}

#define CNANOLOG_ARG_TYPES_2(_1, _2) \
    {CNANOLOG_ARG_TYPE(_1), CNANOLOG_ARG_TYPE(_2)}

#define CNANOLOG_ARG_TYPES_3(_1, _2, _3) \
    {CNANOLOG_ARG_TYPE(_1), CNANOLOG_ARG_TYPE(_2), CNANOLOG_ARG_TYPE(_3)}

#define CNANOLOG_ARG_TYPES_4(_1, _2, _3, _4) \
    {CNANOLOG_ARG_TYPE(_1), CNANOLOG_ARG_TYPE(_2), CNANOLOG_ARG_TYPE(_3), CNANOLOG_ARG_TYPE(_4)}

#define CNANOLOG_ARG_TYPES_5(_1, _2, _3, _4, _5) \
    {CNANOLOG_ARG_TYPE(_1), CNANOLOG_ARG_TYPE(_2), CNANOLOG_ARG_TYPE(_3), CNANOLOG_ARG_TYPE(_4), CNANOLOG_ARG_TYPE(_5)}

#define CNANOLOG_ARG_TYPES_6(_1, _2, _3, _4, _5, _6) \
    {CNANOLOG_ARG_TYPE(_1), CNANOLOG_ARG_TYPE(_2), CNANOLOG_ARG_TYPE(_3), CNANOLOG_ARG_TYPE(_4), CNANOLOG_ARG_TYPE(_5), CNANOLOG_ARG_TYPE(_6)}

#define CNANOLOG_ARG_TYPES_7(_1, _2, _3, _4, _5, _6, _7) \
    {CNANOLOG_ARG_TYPE(_1), CNANOLOG_ARG_TYPE(_2), CNANOLOG_ARG_TYPE(_3), CNANOLOG_ARG_TYPE(_4), CNANOLOG_ARG_TYPE(_5), CNANOLOG_ARG_TYPE(_6), CNANOLOG_ARG_TYPE(_7)}

#define CNANOLOG_ARG_TYPES_8(_1, _2, _3, _4, _5, _6, _7, _8) \
    {CNANOLOG_ARG_TYPE(_1), CNANOLOG_ARG_TYPE(_2), CNANOLOG_ARG_TYPE(_3), CNANOLOG_ARG_TYPE(_4), CNANOLOG_ARG_TYPE(_5), CNANOLOG_ARG_TYPE(_6), CNANOLOG_ARG_TYPE(_7), CNANOLOG_ARG_TYPE(_8)}

/* Add more if needed... For now, limit to 8 args for simplicity */

/* Note: extern "C" opened at line 65 is left open for parent header to close */
