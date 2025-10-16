#include "binary_writer.h"
#include "log_registry.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    /* Create test log */
    binary_writer_t* w = binwriter_create("test.clog");
    binwriter_write_header(w, 1000000000ULL, 0, 1700000000, 0);

    /* Two integers */
    struct { int32_t a, b; } vals = {100, 200};
    binwriter_write_entry(w, 0, 1000, &vals, sizeof(vals));

    log_site_t site = {
        .log_id = 0,
        .log_level = LOG_LEVEL_INFO,
        .filename = "test.c",
        .format = "Values: %d and %d",
        .line_number = 10,
        .num_args = 2,
        .arg_types = {ARG_TYPE_INT32, ARG_TYPE_INT32}
    };
    
    binwriter_close(w, &site, 1);
    
    /* Read back and print raw bytes */
    FILE* fp = fopen("test.clog", "rb");
    fseek(fp, 64, SEEK_SET);  // Skip header
    
    cnanolog_entry_header_t entry;
    fread(&entry, 1, sizeof(entry), fp);
    
    printf("Entry: log_id=%u, "
#ifndef CNANOLOG_NO_TIMESTAMPS
           "timestamp=%llu, "
#endif
           "data_len=%u\n",
           entry.log_id,
#ifndef CNANOLOG_NO_TIMESTAMPS
           entry.timestamp,
#endif
           entry.data_length);
    
    int32_t vals_read[2];
    fread(vals_read, 1, entry.data_length, fp);
    printf("Values: %d, %d\n", vals_read[0], vals_read[1]);
    
    fclose(fp);
    
    /* Decompress */
    printf("\nDecompressor output:\n");
    system("../tools/decompressor test.clog");
    
    return 0;
}
