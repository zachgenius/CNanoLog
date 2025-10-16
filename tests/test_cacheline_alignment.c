/*
 * Test program to verify cache-line alignment
 */

#include "../src/staging_buffer.h"
#include "../src/platform.h"
#include <stdio.h>
#include <stddef.h>

int main(void) {
    printf("Cache-Line Alignment Verification\n");
    printf("===================================\n\n");

    printf("Cache line size: %d bytes\n", CACHE_LINE_SIZE);
    printf("\n");

    /* Check staging_buffer_t alignment */
    printf("staging_buffer_t:\n");
    printf("  sizeof:         %zu bytes\n", sizeof(staging_buffer_t));
    printf("  alignof:        %zu bytes\n", _Alignof(staging_buffer_t));
    printf("  Multiple of 64? %s\n",
           sizeof(staging_buffer_t) % CACHE_LINE_SIZE == 0 ? "YES ✓" : "NO ✗");
    printf("  Aligned to 64?  %s\n",
           _Alignof(staging_buffer_t) == CACHE_LINE_SIZE ? "YES ✓" : "NO ✗");
    printf("\n");

    /* Check field offsets */
    printf("Field offsets:\n");
    printf("  data:           %zu (cache line %zu)\n",
           offsetof(staging_buffer_t, data),
           offsetof(staging_buffer_t, data) / CACHE_LINE_SIZE);
    printf("  write_pos:      %zu (cache line %zu) - PRODUCER (atomic)\n",
           offsetof(staging_buffer_t, write_pos),
           offsetof(staging_buffer_t, write_pos) / CACHE_LINE_SIZE);
    printf("  read_pos:       %zu (cache line %zu) - CONSUMER\n",
           offsetof(staging_buffer_t, read_pos),
           offsetof(staging_buffer_t, read_pos) / CACHE_LINE_SIZE);
    printf("\n");

    /* Verify false sharing prevention */
    size_t write_cacheline = offsetof(staging_buffer_t, write_pos) / CACHE_LINE_SIZE;
    size_t read_cacheline = offsetof(staging_buffer_t, read_pos) / CACHE_LINE_SIZE;
    size_t cacheline_distance = (read_cacheline > write_cacheline)
                                ? (read_cacheline - write_cacheline)
                                : (write_cacheline - read_cacheline);

    printf("False sharing analysis:\n");
    printf("  write_pos vs read_pos:   %s (lines %zu vs %zu)\n",
           write_cacheline != read_cacheline ? "DIFFERENT ✓" : "SAME ✗",
           write_cacheline, read_cacheline);
    printf("  Cache line distance:     %zu lines (%s)\n",
           cacheline_distance,
           cacheline_distance >= 2 ? "EXCELLENT ✓" : "OK");
    printf("  Note: Using atomic write_pos eliminates need for separate 'committed' field\n");
    printf("  This reduces cache line bouncing between producer and consumer!\n");
    printf("\n");

    /* Summary */
    int all_good = 1;
    all_good &= (sizeof(staging_buffer_t) % CACHE_LINE_SIZE == 0);
    all_good &= (_Alignof(staging_buffer_t) == CACHE_LINE_SIZE);
    all_good &= (write_cacheline != read_cacheline);
    all_good &= (cacheline_distance >= 2);  /* At least 2 cache lines apart */

    printf("===================================\n");
    if (all_good) {
        printf("✓ All cache-line alignment checks PASSED!\n");
        printf("✓ False sharing prevention is WORKING!\n");
        return 0;
    } else {
        printf("✗ Some cache-line alignment checks FAILED!\n");
        return 1;
    }
}
