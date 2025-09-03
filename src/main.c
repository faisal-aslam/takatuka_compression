// main.c

#include "constants.h"
#include "logic.h"
#include "timer.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>   // for memset, strcpy, strlen
#include <sys/stat.h> // for file size

long total_input_size;
char *output_file = NULL;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <input_file> <output_file>\n", argv[0]);
        return 1;
    }

    long compressed_size = 0; // accumulate total compressed size

    init_timer();

    // Open input file
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        perror("Failed to open file");
        return 1;
    }

    // Store output filename safely (dynamic, no fixed buffer)
    output_file = malloc(strlen(argv[2]) + 1);
    if (!output_file) {
        perror("Failed to allocate memory for output filename");
        fclose(file);
        return 1;
    }
    strcpy(output_file, argv[2]);

    // Get file size
    fseek(file, 0L, SEEK_END);
    total_input_size = ftell(file);
    rewind(file);

    // Allocate block buffer
    uint8_t *block = malloc(BLOCK_SIZE);
    if (!block) {
        perror("Failed to allocate memory for block");
        free(output_file);
        fclose(file);
        return 1;
    }

    long total_processed = 0;
    int last_percent = -1;

    // 🔹 Process all blocks until EOF
    while (1) {
        size_t bytesRead = fread(block, 1, BLOCK_SIZE, file);
        if (bytesRead == 0) break; // EOF

        // Clear unread tail in final block
        if (bytesRead < BLOCK_SIZE) {
            memset(block + bytesRead, 0, BLOCK_SIZE - bytesRead);
        }

        // Accumulate compressed size for each block
        long size_of_block = process_block(block, (uint32_t)bytesRead);
        compressed_size += size_of_block;

        total_processed += bytesRead;

        // Update progress
        int percent = (int)(((double)total_processed / total_input_size) * 100.0);
        if (percent != last_percent) {
            printf("\rTotal Progress: %3d%%", percent);
            fflush(stdout);
            last_percent = percent;
        }
    }

    printf("\rTotal Progress: 100%%\n");

    if (compressed_size == 0) {
        printf("Error: No data processed.\n");
    } else {
        double ratio = (double)compressed_size / (double)total_input_size;
        printf("Original size:   %ld bytes\n", total_input_size);
        printf("Compressed size: %ld bytes\n", compressed_size);
        printf("Compression ratio: %.2f%%\n", ratio * 100.0);
    }

    free(block);
    free(output_file);
    fclose(file);
    return 0;
}
