#include "../include/cnanolog_types.h"
#include <stdio.h>

int main() {
    printf("COUNT_ARGS() = %d\n", CNANOLOG_COUNT_ARGS());
    printf("COUNT_ARGS(1) = %d\n", CNANOLOG_COUNT_ARGS(1));
    printf("COUNT_ARGS(1,2) = %d\n", CNANOLOG_COUNT_ARGS(1,2));
    return 0;
}
