#pragma once

/* ============================================================================
 * Platform Detection
 * ============================================================================ */

#if defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_WINDOWS
#elif defined(__APPLE__) || defined(__MACH__)
    #define PLATFORM_MACOS
    #define PLATFORM_POSIX
#elif defined(__linux__)
    #define PLATFORM_LINUX
    #define PLATFORM_POSIX
#else
    #error "Unsupported platform"
#endif

/* ============================================================================
 * Cache-Line Alignment
 * ============================================================================ */

/**
 * Cache line size - typically 64 bytes on modern CPUs.
 * Used to prevent false sharing between threads.
 */
#define CACHE_LINE_SIZE 64

/**
 * Macro to align structure to cache-line boundaries.
 * Prevents false sharing between producer and consumer threads.
 */
#if defined(__GNUC__) || defined(__clang__)
    #define ALIGN_CACHELINE __attribute__((aligned(CACHE_LINE_SIZE)))
#elif defined(_MSC_VER)
    #define ALIGN_CACHELINE __declspec(align(CACHE_LINE_SIZE))
#else
    #define ALIGN_CACHELINE
    #warning "Cache-line alignment not supported on this compiler"
#endif

/**
 * Padding to fill remainder of cache line.
 * Usage: char _pad[CACHE_LINE_PAD(sizeof_used_bytes)];
 */
#define CACHE_LINE_PAD(x) (CACHE_LINE_SIZE - ((x) % CACHE_LINE_SIZE))

#ifdef PLATFORM_POSIX
    #include <pthread.h>
    typedef pthread_t cnanolog_thread_t;
    typedef pthread_mutex_t cnanolog_mutex_t;
    typedef pthread_cond_t cnanolog_cond_t;
#elif defined(PLATFORM_WINDOWS)
    #include <windows.h>
    typedef HANDLE cnanolog_thread_t;
    typedef CRITICAL_SECTION cnanolog_mutex_t;
    typedef CONDITION_VARIABLE cnanolog_cond_t;
#endif

// Thread functions
int cnanolog_thread_create(cnanolog_thread_t* thread, void* (*start_routine)(void*), void* arg);
int cnanolog_thread_join(cnanolog_thread_t thread, void** retval);

// Mutex functions
int cnanolog_mutex_init(cnanolog_mutex_t* mutex);
int cnanolog_mutex_lock(cnanolog_mutex_t* mutex);
int cnanolog_mutex_unlock(cnanolog_mutex_t* mutex);
void cnanolog_mutex_destroy(cnanolog_mutex_t* mutex);

// Condition variable functions
int cnanolog_cond_init(cnanolog_cond_t* cond);
int cnanolog_cond_wait(cnanolog_cond_t* cond, cnanolog_mutex_t* mutex);
int cnanolog_cond_signal(cnanolog_cond_t* cond);
void cnanolog_cond_destroy(cnanolog_cond_t* cond);

// CPU affinity functions
int cnanolog_thread_set_affinity(cnanolog_thread_t thread, int core_id);

