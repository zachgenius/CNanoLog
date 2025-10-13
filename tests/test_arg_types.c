/* Test that CNANOLOG_ARG_TYPES macro generates correct types */

#include "../include/cnanolog.h"
#include "../include/cnanolog_format.h"
#include <stdio.h>

int main() {
    printf("Testing CNANOLOG_ARG_TYPES macro:\n\n");

    /* Test 1: Single int */
    int x = 42;
    const uint8_t* types1 = CNANOLOG_ARG_TYPES(x);
    printf("Test 1: int x\n");
    printf("  Types: [%u] (expected [1]=INT32)\n\n", types1[0]);

    /* Test 2: Two ints */
    int a = 10, b = 20;
    const uint8_t* types2 = CNANOLOG_ARG_TYPES(a, b);
    printf("Test 2: int a, int b\n");
    printf("  Types: [%u, %u] (expected [1, 1])\n\n", types2[0], types2[1]);

    /* Test 3: Three ints */
    int i = 10, j = 20, k = 30;
    const uint8_t* types3 = CNANOLOG_ARG_TYPES(i, j, k);
    printf("Test 3: int i, int j, int k\n");
    printf("  Types: [%u, %u, %u] (expected [1, 1, 1])\n\n", types3[0], types3[1], types3[2]);

    /* Test 4: int + string */
    const char* str = "hello";
    int val = 500;
    const uint8_t* types4 = CNANOLOG_ARG_TYPES(val, str);
    printf("Test 4: int val, const char* str\n");
    printf("  Types: [%u, %u] (expected [1, 6]=INT32, STRING)\n\n", types4[0], types4[1]);

    return 0;
}
