// main.c

#include "constants.h"
#include "logic.h"
#include "timer.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h> // for file size

long total_input_size;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }
    init_timer();
    // Open file
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        perror("Failed to open file");
        return 1;
    }

    // Get file size
    fseek(file, 0L, SEEK_END);
    total_input_size = ftell(file);
    rewind(file);

    // Allocate block
    uint8_t *block = malloc(BLOCK_SIZE);
    if (!block) {
        perror("Failed to allocate memory for block");
        fclose(file);
        return 1;
    }

    long total_processed = 0;
    int last_percent = -1;

    // Read and process blocks
    while (1) {
        size_t bytesRead = fread(block, 1, BLOCK_SIZE, file);
        if (bytesRead == 0) break;

        // Clear unread tail in final block
        if (bytesRead < BLOCK_SIZE) {
            memset(block + bytesRead, 0, BLOCK_SIZE - bytesRead);
        }

        process_block(block, (uint32_t)bytesRead);
        total_processed += bytesRead;

        int percent = (int)(((double)total_processed / total_input_size) * 100.0);
        if (percent != last_percent) {
            printf("\rTotal Progress: %3d%%", percent);
            fflush(stdout);
            last_percent = percent;
        }
    }

    printf("\rTotal Progress: 100%%\n");

    free(block);
    fclose(file);
    return 0;
}
