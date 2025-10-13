#include "../include/cnanolog.h"
#include "platform.h"
#include "ring_buffer.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define MAX_LOG_MSG_SIZE 1024

// --- Global State ---
static ring_buffer_t g_ring_buffer;
static mutex_t g_buffer_mutex;
static cond_t g_buffer_cond;
static thread_t g_writer_thread;
static FILE* g_log_file = NULL;
static volatile int g_should_exit = 0;

// --- Forward Declarations ---
static void* writer_thread_main(void* arg);

// --- API Implementation ---

int cnanolog_init(const char* log_file_path) {
    g_log_file = fopen(log_file_path, "a");
    if (!g_log_file) {
        return -1;
    }

    rb_init(&g_ring_buffer);
    mutex_init(&g_buffer_mutex);
    cond_init(&g_buffer_cond);

    if (thread_create(&g_writer_thread, writer_thread_main, NULL) != 0) {
        fclose(g_log_file);
        return -1;
    }

    return 0;
}

void cnanolog_shutdown() {
    g_should_exit = 1;
    cond_signal(&g_buffer_cond); // Wake up the writer thread to exit
    thread_join(g_writer_thread, NULL);

    // Final flush
    char temp_buf[MAX_LOG_MSG_SIZE];
    while (rb_read(&g_ring_buffer, temp_buf, MAX_LOG_MSG_SIZE) > 0) {
        fputs(temp_buf, g_log_file);
    }

    if (g_log_file) {
        fclose(g_log_file);
    }

    mutex_destroy(&g_buffer_mutex);
    cond_destroy(&g_buffer_cond);
}

void _cnanolog_log_internal(cnanolog_level_t level, const char* file, int line, const char* format, ...) {
    if (!g_log_file) return;

    char temp_buf[MAX_LOG_MSG_SIZE];
    char final_buf[MAX_LOG_MSG_SIZE];
    const char* level_str;

    switch (level) {
        case LOG_LEVEL_INFO:  level_str = "INFO";  break;
        case LOG_LEVEL_WARN:  level_str = "WARN";  break;
        case LOG_LEVEL_ERROR: level_str = "ERROR"; break;
        case LOG_LEVEL_DEBUG: level_str = "DEBUG"; break;
        default:              level_str = "????";  break;
    }

    // 1. Format the user's message
    va_list args;
    va_start(args, format);
    vsnprintf(temp_buf, sizeof(temp_buf), format, args);
    va_end(args);

    // 2. Format the final log line with timestamp, level, etc.
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    int header_len = snprintf(final_buf, sizeof(final_buf),
                            "[%04d-%02d-%02d %02d:%02d:%02d] [%s] [%s:%d] %s\n",
                            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                            t->tm_hour, t->tm_min, t->tm_sec,
                            level_str, file, line, temp_buf);

    // 3. Write to the ring buffer
    mutex_lock(&g_buffer_mutex);
    if (rb_write(&g_ring_buffer, final_buf, header_len) != 0) {
        // Optional: Handle buffer full case. For this MVP, we just drop the log.
    }
    mutex_unlock(&g_buffer_mutex);

    // 4. Signal the writer thread
    cond_signal(&g_buffer_cond);
}

// --- Background Thread ---

static void* writer_thread_main(void* arg) {
    (void)arg; // Unused
    char temp_buf[MAX_LOG_MSG_SIZE];

    while (!g_should_exit) {
        mutex_lock(&g_buffer_mutex);
        while (!g_should_exit && g_ring_buffer.read_pos == g_ring_buffer.write_pos && !g_ring_buffer.is_full) {
            cond_wait(&g_buffer_cond, &g_buffer_mutex);
        }

        // We were woken up. Read all available messages.
        while (rb_read(&g_ring_buffer, temp_buf, MAX_LOG_MSG_SIZE) > 0) {
            // Unlock while writing to the file to allow producers to log more messages.
            mutex_unlock(&g_buffer_mutex);
            fputs(temp_buf, g_log_file);
            mutex_lock(&g_buffer_mutex);
        }
        mutex_unlock(&g_buffer_mutex);
        fflush(g_log_file);
    }

    return NULL;
}

