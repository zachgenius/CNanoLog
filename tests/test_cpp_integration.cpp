/*
 * Test CNanoLog C++ integration
 *
 * Verifies that CNanoLog can be used from C++ code with:
 * - Automatic type detection via template overloading
 * - Mixed argument types (int, string, double, etc.)
 * - C++ string literals and types
 */

#include "../include/cnanolog.h"
#include <cstdio>
#include <cstring>

int main() {
    printf("Testing CNanoLog C++ integration...\n");

    /* Initialize logger */
    if (cnanolog_init("test_cpp.clog") != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    /* Test basic logging with automatic type detection */
    LOG_INFO("C++ test: No arguments");

    /* Test with integers */
    LOG_INFO("C++ test: int = %d", 42);
    LOG_INFO("C++ test: two ints = %d %d", 123, 456);

    /* Test with strings */
    LOG_INFO("C++ test: string = %s", "hello from C++");
    const char* message = "const char*";
    LOG_INFO("C++ test: const string = %s", message);

    /* Test with mixed types - this is the key C++ test */
    LOG_INFO("C++ test: int and string = %d %s", 789, "mixed");
    LOG_INFO("C++ test: string and int = %s %d", "reversed", 999);

    /* Test with doubles */
    LOG_INFO("C++ test: double = %f", 3.14159);
    LOG_INFO("C++ test: int and double = %d %f", 42, 2.718);

    /* Test with different log levels */
    LOG_WARN("C++ warning: code = %d", 100);
    LOG_ERROR("C++ error: code = %d", 500);
    LOG_DEBUG("C++ debug: x=%d y=%d", 10, 20);

    /* Test with more arguments */
    LOG_INFO("C++ test: three args = %d %s %f", 1, "two", 3.0);
    LOG_INFO("C++ test: four args = %d %d %d %d", 1, 2, 3, 4);

    /* Test with unsigned types */
    unsigned int u32 = 0xDEADBEEF;
    unsigned long long u64 = 0xCAFEBABEDEADBEEFULL;
    LOG_INFO("C++ test: unsigned int = 0x%x", u32);
    LOG_INFO("C++ test: unsigned long long = 0x%llx", u64);

    /* Test with pointers */
    void* ptr = (void*)0x12345678;
    LOG_INFO("C++ test: pointer = %p", ptr);

    /* Get statistics */
    cnanolog_stats_t stats;
    cnanolog_get_stats(&stats);
    printf("Logs written: %llu\n", (unsigned long long)stats.total_logs_written);

    /* Shutdown */
    cnanolog_shutdown();

    printf("C++ integration test passed!\n");
    return 0;
}
