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
    printf("  write_pos:      %zu (cache line %zu) - PRODUCER\n",
           offsetof(staging_buffer_t, write_pos),
           offsetof(staging_buffer_t, write_pos) / CACHE_LINE_SIZE);
    printf("  committed:      %zu (cache line %zu) - SHARED\n",
           offsetof(staging_buffer_t, committed),
           offsetof(staging_buffer_t, committed) / CACHE_LINE_SIZE);
    printf("  read_pos:       %zu (cache line %zu) - CONSUMER\n",
           offsetof(staging_buffer_t, read_pos),
           offsetof(staging_buffer_t, read_pos) / CACHE_LINE_SIZE);
    printf("\n");

    /* Verify false sharing prevention */
    size_t write_cacheline = offsetof(staging_buffer_t, write_pos) / CACHE_LINE_SIZE;
    size_t commit_cacheline = offsetof(staging_buffer_t, committed) / CACHE_LINE_SIZE;
    size_t read_cacheline = offsetof(staging_buffer_t, read_pos) / CACHE_LINE_SIZE;

    printf("False sharing analysis:\n");
    printf("  write_pos vs committed:  %s (lines %zu vs %zu)\n",
           write_cacheline != commit_cacheline ? "DIFFERENT ✓" : "SAME ✗",
           write_cacheline, commit_cacheline);
    printf("  committed vs read_pos:   %s (lines %zu vs %zu)\n",
           commit_cacheline != read_cacheline ? "DIFFERENT ✓" : "SAME ✗",
           commit_cacheline, read_cacheline);
    printf("  write_pos vs read_pos:   %s (lines %zu vs %zu)\n",
           write_cacheline != read_cacheline ? "DIFFERENT ✓" : "SAME ✗",
           write_cacheline, read_cacheline);
    printf("\n");

    /* Summary */
    int all_good = 1;
    all_good &= (sizeof(staging_buffer_t) % CACHE_LINE_SIZE == 0);
    all_good &= (_Alignof(staging_buffer_t) == CACHE_LINE_SIZE);
    all_good &= (write_cacheline != commit_cacheline);
    all_good &= (commit_cacheline != read_cacheline);
    all_good &= (write_cacheline != read_cacheline);

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
