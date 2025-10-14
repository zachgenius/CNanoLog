#include <stdio.h>

/* Linux-specific headers for CPU affinity */
#if defined(__linux__)
    #define _GNU_SOURCE
    #include <sched.h>
#endif

/* macOS-specific headers for CPU affinity - include BEFORE platform.h */
#if defined(__APPLE__) || defined(__MACH__)
    #include <mach/mach.h>
    #include <mach/thread_policy.h>
    #include <mach/thread_act.h>
#endif

/* Now include platform.h which will see the mach types */
#include "platform.h"

#ifdef PLATFORM_POSIX

// Thread functions
int cnanolog_thread_create(cnanolog_thread_t* thread, void* (*start_routine)(void*), void* arg) {
    return pthread_create(thread, NULL, start_routine, arg);
}

int cnanolog_thread_join(cnanolog_thread_t thread, void** retval) {
    return pthread_join(thread, retval);
}

// Mutex functions
int cnanolog_mutex_init(cnanolog_mutex_t* mutex) {
    return pthread_mutex_init(mutex, NULL);
}

int cnanolog_mutex_lock(cnanolog_mutex_t* mutex) {
    return pthread_mutex_lock(mutex);
}

int cnanolog_mutex_unlock(cnanolog_mutex_t* mutex) {
    return pthread_mutex_unlock(mutex);
}

void cnanolog_mutex_destroy(cnanolog_mutex_t* mutex) {
    pthread_mutex_destroy(mutex);
}

// Condition variable functions
int cnanolog_cond_init(cnanolog_cond_t* cond) {
    return pthread_cond_init(cond, NULL);
}

int cnanolog_cond_wait(cnanolog_cond_t* cond, cnanolog_mutex_t* mutex) {
    return pthread_cond_wait(cond, mutex);
}

int cnanolog_cond_signal(cnanolog_cond_t* cond) {
    return pthread_cond_signal(cond);
}

void cnanolog_cond_destroy(cnanolog_cond_t* cond) {
    pthread_cond_destroy(cond);
}

// CPU affinity functions
int cnanolog_thread_set_affinity(cnanolog_thread_t thread, int core_id) {
    if (core_id < 0) {
        return -1;  /* Invalid core ID */
    }

#ifdef PLATFORM_LINUX
    /* Linux: Use pthread_setaffinity_np */
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    int result = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        fprintf(stderr, "Warning: Failed to set thread affinity to core %d (error %d)\n",
                core_id, result);
        return -1;
    }
    return 0;

#elif defined(PLATFORM_MACOS)
    /* macOS: Use thread_policy_set with thread affinity policy */
    /* Note: macOS thread affinity is more complex and less direct than Linux */
    /* macOS prefers thread_policy_set with THREAD_AFFINITY_POLICY */

    /* Get the Mach thread port from pthread_t */
    mach_port_t mach_thread = pthread_mach_thread_np(thread);

    /* Set up affinity tag (macOS uses affinity tags, not direct core IDs) */
    thread_affinity_policy_data_t policy;
    policy.affinity_tag = core_id;  /* Use core_id as affinity tag */

    kern_return_t result = thread_policy_set(
        mach_thread,
        THREAD_AFFINITY_POLICY,
        (thread_policy_t)&policy,
        THREAD_AFFINITY_POLICY_COUNT
    );

    if (result != KERN_SUCCESS) {
        fprintf(stderr, "Warning: Failed to set thread affinity on macOS (error %d)\n", result);
        fprintf(stderr, "         macOS thread affinity is best-effort and may not pin to exact core\n");
        return -1;
    }

    return 0;
#else
    /* Other POSIX systems - affinity not supported */
    fprintf(stderr, "Warning: Thread affinity not supported on this platform\n");
    return -1;
#endif
}

#elif defined(PLATFORM_WINDOWS)

// Thread functions
int cnanolog_thread_create(cnanolog_thread_t* thread, void* (*start_routine)(void*), void* arg) {
    *thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)start_routine, arg, 0, NULL);
    return (*thread == NULL) ? -1 : 0;
}

int cnanolog_thread_join(cnanolog_thread_t thread, void** retval) {
    WaitForSingleObject(thread, INFINITE);
    // Note: Getting a return value from a thread in Windows is more complex
    // and often requires a shared structure. For this MVP, we don't need it.
    if (retval) {
        *retval = 0; // Placeholder
    }
    CloseHandle(thread);
    return 0;
}

// Mutex functions
int cnanolog_mutex_init(cnanolog_mutex_t* mutex) {
    InitializeCriticalSection(mutex);
    return 0;
}

int cnanolog_mutex_lock(cnanolog_mutex_t* mutex) {
    EnterCriticalSection(mutex);
    return 0;
}

int cnanolog_mutex_unlock(cnanolog_mutex_t* mutex) {
    LeaveCriticalSection(mutex);
    return 0;
}

void cnanolog_mutex_destroy(cnanolog_mutex_t* mutex) {
    DeleteCriticalSection(mutex);
}

// Condition variable functions
int cnanolog_cond_init(cnanolog_cond_t* cond) {
    InitializeConditionVariable(cond);
    return 0;
}

int cnanolog_cond_wait(cnanolog_cond_t* cond, cnanolog_mutex_t* mutex) {
    return SleepConditionVariableCS(cond, mutex, INFINITE) ? 0 : -1;
}

int cnanolog_cond_signal(cnanolog_cond_t* cond) {
    WakeConditionVariable(cond);
    return 0;
}

void cnanolog_cond_destroy(cnanolog_cond_t* cond) {
    // No-op on Windows
}

// CPU affinity functions
int cnanolog_thread_set_affinity(cnanolog_thread_t thread, int core_id) {
    if (core_id < 0) {
        return -1;  /* Invalid core ID */
    }

    /* Windows: Use SetThreadAffinityMask */
    DWORD_PTR affinity_mask = (DWORD_PTR)1 << core_id;
    DWORD_PTR result = SetThreadAffinityMask(thread, affinity_mask);

    if (result == 0) {
        fprintf(stderr, "Warning: Failed to set thread affinity to core %d (error %lu)\n",
                core_id, GetLastError());
        return -1;
    }

    return 0;
}

#endif
