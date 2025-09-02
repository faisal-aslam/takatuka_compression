// main.c

#include "constants.h"
#include "logic.h"
#include "timer.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>   // for memset
#include <sys/stat.h> // for file size

long total_input_size;
char output_file[500];

// assume: extern double alpha, beta;
static int ab_step = 2;

int next_alpha_beta(void) {
    // try alpha from 0.1 → 0.9 (beta = 1 - alpha)
    if (ab_step >= 3) return 0; // no more
    WEIGHT_FREQ = (ab_step + 1) / 10.0;
    WEIGHT_LEN = 1.0 - WEIGHT_FREQ;
    ab_step++;
    return 1; // more left
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <input_file> <output_file>\n", argv[0]);
        return 1;
    }
    long best_size = LONG_MAX;
    double best_alpha = 0, best_beta = 0;

    init_timer();
    // Open file
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        perror("Failed to open file");
        return 1;
    }
    memcpy(output_file, argv[2], sizeof(char) * 500);

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

    // Sweep alpha/beta values
    while (next_alpha_beta()) {
        printf("Trying alpha=%.2f beta=%.2f\n", WEIGHT_FREQ, WEIGHT_LEN);

        // 🔹 In the future, wrap the following fread() and block handling
        //     inside another `while (...) {}` to process multiple blocks.
        // For now, we only test with one block.
        rewind(file); // restart from beginning for each alpha/beta trial
        size_t bytesRead = fread(block, 1, BLOCK_SIZE, file);
        if (bytesRead == 0) break;

        // Clear unread tail in final block
        if (bytesRead < BLOCK_SIZE) {
            memset(block + bytesRead, 0, BLOCK_SIZE - bytesRead);
        }

        // 🔹 In future: accumulate results over multiple blocks instead of
        //     calling process_block() once here.
        long size_of_file = process_block(block, (uint32_t)bytesRead);
        if (size_of_file < best_size) {
            best_size = size_of_file;
            best_alpha = WEIGHT_FREQ;
            best_beta = WEIGHT_LEN;
        }
        total_processed += bytesRead;

        // 🔹 In future: update progress after each block in the inner loop.
        int percent = (int)(((double)total_processed / total_input_size) * 100.0);
        if (percent != last_percent) {
            printf("\rTotal Progress: %3d%%", percent);
            fflush(stdout);
            last_percent = percent;
        }

        // For now: since only one block is processed, always show 100%
        printf("\rTotal Progress: 100%%\n");
        printf("Best alpha=%.2f beta=%.2f size=%ld\n",
               best_alpha, best_beta, best_size);
    }

    free(block);   // 🔹 In future: free after processing *all* blocks.
    fclose(file);
    return 0;
}
