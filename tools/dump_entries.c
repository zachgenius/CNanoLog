/* Dump binary log file entries with clear boundaries */

#include "../include/cnanolog_format.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void dump_hex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x ", data[i]);
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <logfile.clog>\n", argv[0]);
        return 1;
    }

    FILE* fp = fopen(argv[1], "rb");
    if (!fp) {
        fprintf(stderr, "Cannot open %s\n", argv[1]);
        return 1;
    }

    /* Read header */
    cnanolog_file_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fprintf(stderr, "Failed to read header\n");
        fclose(fp);
        return 1;
    }

    printf("File Header:\n");
    printf("  Magic: %.4s\n", (char*)&header.magic);
    printf("  Timestamp freq: %llu\n", header.timestamp_frequency);
    printf("  Start timestamp: %llu\n", header.start_timestamp);
    printf("\n");

    /* Skip to dictionary at end of file (if dictionary_offset != 0) */
    if (header.dictionary_offset > 0) {
        fseek(fp, header.dictionary_offset, SEEK_SET);
    } else {
        /* Dictionary is at end, seek to end minus dict size */
        /* For now, just skip reading dictionary and go straight to entries */
    }

    /* Seek back to start of entries (after file header) */
    fseek(fp, sizeof(cnanolog_file_header_t), SEEK_SET);

    /* Read log entries */
    printf("Log Entries:\n");
    printf("============\n\n");

    int entry_num = 0;
    while (1) {
        cnanolog_entry_header_t entry_header;
        if (fread(&entry_header, sizeof(entry_header), 1, fp) != 1) {
            break;  /* End of file */
        }

        printf("Entry #%d:\n", entry_num);
        printf("  log_id: %u\n", entry_header.log_id);
        printf("  timestamp: %llu\n", entry_header.timestamp);
        printf("  data_length: %u bytes\n", entry_header.data_length);

        if (entry_header.data_length > 0) {
            uint8_t* data = malloc(entry_header.data_length);
            if (fread(data, 1, entry_header.data_length, fp) != entry_header.data_length) {
                fprintf(stderr, "Failed to read entry data\n");
                free(data);
                break;
            }

            printf("  data (hex): ");
            dump_hex(data, entry_header.data_length);
            printf("\n");
            free(data);
        }

        printf("\n");
        entry_num++;
    }

    fclose(fp);
    return 0;
}
