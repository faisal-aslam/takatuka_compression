// rle_main.c

#include "sort.h"
#include "bitmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define BLOCK_SIZE 32
#define BIT_MAP_THREASHOLD (BLOCK_SIZE*8/2)
#define COMPRESSION_THRESHOLD 10.0  // lower for testing

extern SortInfo sort_info;

static void print_block(const uint8_t *data, int block_size) {
    for (int i = 0; i < block_size; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

// -------------------- Delta + RLE size calculation --------------------

// How many bytes needed for variable-length encoding of x
static int varint_size(unsigned int x) {
    int size = 0;
    do {
        x >>= 7;
        size++;
    } while (x);
    return size;
}

// Compute compressed size of a block using Delta + RLE
static int delta_rle_size(const uint8_t *block, int n) {
    if (n <= 0) return 0;

    int size_bytes = 1; // store first value as-is
    int prev = block[0];

    for (int i = 1; i < n; ) {
        int run_val = block[i];
        int run_len = 1;

        // count run length
        while (i + run_len < n && block[i + run_len] == run_val) {
            run_len++;
        }

        int delta = run_val - prev;

        if (run_len > 1) {
            // If there’s an actual run: store delta + count
            size_bytes += varint_size(delta) + varint_size(run_len);
        } else {
            // Single value: only store delta
            size_bytes += varint_size(delta);
        }

        prev = run_val;
        i += run_len;
    }

    return size_bytes;
}

// ---------------------------------------------------------------------

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s input.bin\n", argv[0]);
        return 1;
    }

    uint8_t *data = NULL;
    size_t size = 0;
    if (!read_input_file(argv[1], &data, &size)) {
        fprintf(stderr, "Failed to read input file %s\n", argv[1]);
        return 1;
    }

    printf("Input size = %zu bytes\n", size);
    printf("Fixed block size = %d\n", BLOCK_SIZE);
    printf("Bitmap threshold = %d\n", BIT_MAP_THREASHOLD);
    printf("Compression threshold = %.1f%%\n\n", COMPRESSION_THRESHOLD);

    size_t accepted_blocks = 0;
    size_t total_blocks = 0;

    for (size_t i = 0; i + BLOCK_SIZE <= size; ) {
        total_blocks++;

        // Copy block
        uint8_t *block = malloc(BLOCK_SIZE);
        memcpy(block, data + i, BLOCK_SIZE);

        // Reset sort info
        sort_info.original_size = BLOCK_SIZE;
        sort_info.bitmap_size = 0;

        // Sort block (needed for bitmap + RLE effectiveness)
        merge_sort(block, BLOCK_SIZE, BLOCK_SIZE);

        if (sort_info.bitmap_size <= BIT_MAP_THREASHOLD) {
            // Run Delta+RLE test
            int compressed_size = delta_rle_size(block, BLOCK_SIZE);
            double gain = 100.0 * (1.0 - (double)compressed_size / BLOCK_SIZE);

            // Debug info for every block
            printf("Block %zu: orig=%d, compressed=%d, gain=%.2f%%\n",
                   i, BLOCK_SIZE, compressed_size, gain);

            if (gain >= COMPRESSION_THRESHOLD) {
                accepted_blocks++;
                printf("  -> ACCEPTED\n");
            } else {
                printf("  -> REJECTED by compression\n");
            }

            // Optional: print block & bitmap
            print_block(data + i, BLOCK_SIZE);
            print_bitmap(sort_info.bitmap, sort_info.bitmap_size);
            printf("\n");

            i += BLOCK_SIZE;  // skip whole block
        } else {
            printf("Block %zu: IGNORED by bitmap (bitmap=%zu)\n",
                   i, sort_info.bitmap_size);

            print_block(data + i, BLOCK_SIZE);
            print_bitmap(sort_info.bitmap, sort_info.bitmap_size);
            printf("\n");

            i += 1;  // slide window by one
        }

        free(block);
    }

    double percent = (total_blocks > 0)
                     ? (100.0 * accepted_blocks / total_blocks)
                     : 0.0;

    printf("\n=== SUMMARY ===\n");
    printf("Total blocks processed: %zu\n", total_blocks);
    printf("Accepted blocks: %zu\n", accepted_blocks);
    printf("Acceptance rate: %.2f%%\n", percent);

    free(data);
    return 0;
}
