// main.c


#include <stdlib.h>
#include <stdio.h>
#include "logic.h"
#include "constants.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        // printf("Usage: %s <input_file> <output_file>\n", argv[0]);
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        perror("Failed to open file");
        return 1;
    }

    uint8_t *block = malloc(BLOCK_SIZE);
    if (!block) {
        perror("Failed to allocate memory for block");
        fclose(file);
        return 1;
    }

    while (1) {
        long bytesRead = fread(block, 1, BLOCK_SIZE, file);
        if (bytesRead <= 0)
            break;

        // process of block of file at a time.
        process_block(block, bytesRead);


    }

    free(block);
    block = NULL;

    fclose(file);

    return 0;
}
