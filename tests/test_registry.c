/* Test for Log Registry and Type Detection */

#include "../src/log_registry.h"
#include "../include/cnanolog_types.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

void test_registry_init() {
    printf("Test: Registry initialization\n");
    log_registry_t registry;
    log_registry_init(&registry);

    assert(registry.count == 0);
    assert(registry.capacity > 0);
    assert(registry.sites != NULL);

    log_registry_destroy(&registry);
    printf("  ✓ Registry init/destroy\n");
}

void test_registry_register() {
    printf("Test: Log site registration\n");
    log_registry_t registry;
    log_registry_init(&registry);

    uint8_t arg_types[] = {ARG_TYPE_INT32, ARG_TYPE_STRING};
    uint32_t id = log_registry_register(&registry, LOG_LEVEL_INFO,
                                         "test.c", 10,
                                         "Count: %d, Name: %s",
                                         2, arg_types);

    assert(id == 0);  /* First registration should get ID 0 */
    assert(log_registry_count(&registry) == 1);

    const log_site_t* site = log_registry_get(&registry, id);
    assert(site != NULL);
    assert(site->log_id == 0);
    assert(site->log_level == LOG_LEVEL_INFO);
    assert(site->line_number == 10);
    assert(site->num_args == 2);
    assert(site->arg_types[0] == ARG_TYPE_INT32);
    assert(site->arg_types[1] == ARG_TYPE_STRING);
    assert(strcmp(site->filename, "test.c") == 0);
    assert(strcmp(site->format, "Count: %d, Name: %s") == 0);

    log_registry_destroy(&registry);
    printf("  ✓ Basic registration\n");
}

void test_registry_duplicate() {
    printf("Test: Duplicate site detection\n");
    log_registry_t registry;
    log_registry_init(&registry);

    uint8_t arg_types[] = {ARG_TYPE_INT32};

    /* Register same site twice */
    uint32_t id1 = log_registry_register(&registry, LOG_LEVEL_INFO,
                                          "test.c", 10,
                                          "Message: %d", 1, arg_types);

    uint32_t id2 = log_registry_register(&registry, LOG_LEVEL_INFO,
                                          "test.c", 10,
                                          "Message: %d", 1, arg_types);

    assert(id1 == id2);  /* Should return same ID */
    assert(log_registry_count(&registry) == 1);  /* Only one entry */

    /* Register different site (different line) */
    uint32_t id3 = log_registry_register(&registry, LOG_LEVEL_INFO,
                                          "test.c", 20,
                                          "Message: %d", 1, arg_types);

    assert(id3 != id1);  /* Should get different ID */
    assert(log_registry_count(&registry) == 2);

    log_registry_destroy(&registry);
    printf("  ✓ Duplicate detection\n");
}

void test_registry_multiple() {
    printf("Test: Multiple site registration\n");
    log_registry_t registry;
    log_registry_init(&registry);

    uint8_t types1[] = {ARG_TYPE_INT32};
    uint8_t types2[] = {ARG_TYPE_STRING};
    uint8_t types3[] = {ARG_TYPE_INT32, ARG_TYPE_DOUBLE};

    uint32_t id1 = log_registry_register(&registry, LOG_LEVEL_INFO, "a.c", 10, "Msg1: %d", 1, types1);
    uint32_t id2 = log_registry_register(&registry, LOG_LEVEL_WARN, "b.c", 20, "Msg2: %s", 1, types2);
    uint32_t id3 = log_registry_register(&registry, LOG_LEVEL_ERROR, "c.c", 30, "Msg3: %d %f", 2, types3);

    assert(id1 == 0);
    assert(id2 == 1);
    assert(id3 == 2);
    assert(log_registry_count(&registry) == 3);

    /* Verify each site */
    const log_site_t* site1 = log_registry_get(&registry, id1);
    assert(site1->log_level == LOG_LEVEL_INFO);
    assert(strcmp(site1->filename, "a.c") == 0);

    const log_site_t* site2 = log_registry_get(&registry, id2);
    assert(site2->log_level == LOG_LEVEL_WARN);
    assert(strcmp(site2->filename, "b.c") == 0);

    const log_site_t* site3 = log_registry_get(&registry, id3);
    assert(site3->log_level == LOG_LEVEL_ERROR);
    assert(strcmp(site3->filename, "c.c") == 0);
    assert(site3->num_args == 2);

    log_registry_destroy(&registry);
    printf("  ✓ Multiple sites\n");
}

void test_type_detection() {
    printf("Test: Type detection macros\n");

    int i = 42;
    long long ll = 123LL;
    unsigned int ui = 100U;
    unsigned long long ull = 200ULL;
    float f = 3.14f;
    double d = 2.71;
    const char* s = "hello";
    void* p = NULL;

    /* Test individual type detection */
    assert(CNANOLOG_ARG_TYPE(i) == ARG_TYPE_INT32);
    assert(CNANOLOG_ARG_TYPE(ll) == ARG_TYPE_INT64);
    assert(CNANOLOG_ARG_TYPE(ui) == ARG_TYPE_UINT32);
    assert(CNANOLOG_ARG_TYPE(ull) == ARG_TYPE_UINT64);
    assert(CNANOLOG_ARG_TYPE(f) == ARG_TYPE_DOUBLE);
    assert(CNANOLOG_ARG_TYPE(d) == ARG_TYPE_DOUBLE);
    assert(CNANOLOG_ARG_TYPE(s) == ARG_TYPE_STRING);
    assert(CNANOLOG_ARG_TYPE(p) == ARG_TYPE_POINTER);

    printf("  ✓ Type detection\n");
}

void test_arg_counting() {
    printf("Test: Argument counting\n");

    /* Test various argument counts
     * Note: Zero arguments is tricky with preprocessor, but we don't need it
     * since log calls always have at least a format string as an argument.
     */
    assert(CNANOLOG_COUNT_ARGS(1) == 1);
    assert(CNANOLOG_COUNT_ARGS(1, 2) == 2);
    assert(CNANOLOG_COUNT_ARGS(1, 2, 3) == 3);
    assert(CNANOLOG_COUNT_ARGS(1, 2, 3, 4) == 4);
    assert(CNANOLOG_COUNT_ARGS(1, 2, 3, 4, 5) == 5);

    printf("  ✓ Argument counting\n");
}

void test_arg_type_array() {
    printf("Test: Argument type array building\n");

    int x = 10;
    const char* name = "test";

    /* Build type array */
    uint8_t* types1 = CNANOLOG_ARG_TYPES(x);
    assert(types1[0] == ARG_TYPE_INT32);

    uint8_t* types2 = CNANOLOG_ARG_TYPES(x, name);
    assert(types2[0] == ARG_TYPE_INT32);
    assert(types2[1] == ARG_TYPE_STRING);

    double val = 3.14;
    uint8_t* types3 = CNANOLOG_ARG_TYPES(x, name, val);
    assert(types3[0] == ARG_TYPE_INT32);
    assert(types3[1] == ARG_TYPE_STRING);
    assert(types3[2] == ARG_TYPE_DOUBLE);

    printf("  ✓ Type array building\n");
}

int main() {
    printf("CNanoLog Registry & Type Detection Tests\n");
    printf("=========================================\n\n");

    test_registry_init();
    test_registry_register();
    test_registry_duplicate();
    test_registry_multiple();
    test_type_detection();
    test_arg_counting();
    test_arg_type_array();

    printf("\n=========================================\n");
    printf("✓ All tests PASSED\n");

    return 0;
}
