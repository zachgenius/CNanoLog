#include "platform.h"

#ifdef PLATFORM_POSIX

// Thread functions
int thread_create(thread_t* thread, void* (*start_routine)(void*), void* arg) {
    return pthread_create(thread, NULL, start_routine, arg);
}

int thread_join(thread_t thread, void** retval) {
    return pthread_join(thread, retval);
}

// Mutex functions
int mutex_init(mutex_t* mutex) {
    return pthread_mutex_init(mutex, NULL);
}

int mutex_lock(mutex_t* mutex) {
    return pthread_mutex_lock(mutex);
}

int mutex_unlock(mutex_t* mutex) {
    return pthread_mutex_unlock(mutex);
}

void mutex_destroy(mutex_t* mutex) {
    pthread_mutex_destroy(mutex);
}

// Condition variable functions
int cond_init(cond_t* cond) {
    return pthread_cond_init(cond, NULL);
}

int cond_wait(cond_t* cond, mutex_t* mutex) {
    return pthread_cond_wait(cond, mutex);
}

int cond_signal(cond_t* cond) {
    return pthread_cond_signal(cond);
}

void cond_destroy(cond_t* cond) {
    pthread_cond_destroy(cond);
}

#elif defined(PLATFORM_WINDOWS)

// Thread functions
int thread_create(thread_t* thread, void* (*start_routine)(void*), void* arg) {
    *thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)start_routine, arg, 0, NULL);
    return (*thread == NULL) ? -1 : 0;
}

int thread_join(thread_t thread, void** retval) {
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
int mutex_init(mutex_t* mutex) {
    InitializeCriticalSection(mutex);
    return 0;
}

int mutex_lock(mutex_t* mutex) {
    EnterCriticalSection(mutex);
    return 0;
}

int mutex_unlock(mutex_t* mutex) {
    LeaveCriticalSection(mutex);
    return 0;
}

void mutex_destroy(mutex_t* mutex) {
    DeleteCriticalSection(mutex);
}

// Condition variable functions
int cond_init(cond_t* cond) {
    InitializeConditionVariable(cond);
    return 0;
}

int cond_wait(cond_t* cond, mutex_t* mutex) {
    return SleepConditionVariableCS(cond, mutex, INFINITE) ? 0 : -1;
}

int cond_signal(cond_t* cond) {
    WakeConditionVariable(cond);
    return 0;
}

void cond_destroy(cond_t* cond) {
    // No-op on Windows
}

#endif
