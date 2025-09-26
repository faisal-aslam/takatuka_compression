#include "sort.h"
#include "bitmap.h"
#include "suitable_sequences.h" 
#include "fileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define BLOCK_SIZE 256
#define BIT_MAP_THREASHOLD (BLOCK_SIZE*8/2)
#define COMPRESSION_THRESHOLD 10.0
#define MIN_SUBBLOCK_SIZE 10
#define DENSITY 1.0   // tune for suitable_sequences()

extern SortInfo sort_info;

// -------------------- Delta + RLE size --------------------
static int varint_size(unsigned int x) {
    int size = 0;
    do { x >>= 7; size++; } while (x);
    return size;
}

static int delta_rle_size(const uint8_t *block, int n) {
    if (n <= 0) return 0;

    int size_bytes = 1;
    int prev = block[0];

    for (int i = 1; i < n; ) {
        int run_val = block[i];
        int run_len = 1;

        while (i + run_len < n && block[i + run_len] == run_val) {
            run_len++;
        }

        int delta = run_val - prev;
        if (run_len > 1) {
            size_bytes += varint_size(delta) + varint_size(run_len);
        } else {
            size_bytes += varint_size(delta);
        }

        prev = run_val;
        i += run_len;
    }

    return size_bytes;
}

// -------------------- Debug printing --------------------
static void print_block(const uint8_t *data, int n) {
    for (int i = 0; i < n; i++) {
        printf("%3d ", data[i]);
    }
    printf("\n");
}

// -------------------- Main --------------------
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

    size_t accepted_blocks = 0, total_blocks = 0;
    size_t total_orig_bytes = 0;
    size_t total_compressed_bytes = 0;

    for (size_t i = 0; i + BLOCK_SIZE <= size; i += BLOCK_SIZE) {
        total_blocks++;

        uint8_t *block = data + i;
        int best_start, best_end;

        if (!find_suitable_subblock(block, BLOCK_SIZE,
                                    DENSITY, MIN_SUBBLOCK_SIZE,
                                    &best_start, &best_end)) {
            printf("Block %zu: no suitable subblock\n\n", i / BLOCK_SIZE);
            continue;
        }

        int sub_len = best_end - best_start + 1;
        printf("Block %zu: found subblock [%d..%d], len=%d\n",
               i / BLOCK_SIZE, best_start, best_end, sub_len);

        // Copy subblock and sort it
        uint8_t *subblock = malloc(sub_len);
        memcpy(subblock, block + best_start, sub_len);

        sort_info.original_size = sub_len;
        sort_info.bitmap_size = 0;
        merge_sort(subblock, sub_len, sub_len);

        // Compress sorted subblock
        int compressed_size = delta_rle_size(subblock, sub_len);

        // ✅ Include bitmap size
        int effective_size = compressed_size /*+ sort_info.bitmap_size/8*/;
        double gain = 100.0 * (1.0 - (double)effective_size / sub_len);

        printf("  orig=%d, compressed=%d, bitmap=%zu, effective=%d, gain=%.2f%%\n",
               sub_len, compressed_size, sort_info.bitmap_size,
               effective_size, gain);

        total_orig_bytes += sub_len;
        total_compressed_bytes += effective_size;

        if (gain >= COMPRESSION_THRESHOLD) {
            accepted_blocks++;
            printf("  -> ACCEPTED\n");
        } else {
            printf("  -> REJECTED\n");
        }

        print_block(subblock, sub_len);
        print_bitmap(sort_info.bitmap, sort_info.bitmap_size);
        printf("\n");

        free(subblock);
    }

    // Final stats
    printf("\n=== SUMMARY ===\n");
    printf("Total blocks processed: %zu\n", total_blocks);
    printf("Accepted blocks: %zu\n", accepted_blocks);
    printf("Acceptance rate: %.2f%%\n",
           total_blocks ? 100.0 * accepted_blocks / total_blocks : 0.0);

    if (total_orig_bytes > 0) {
        double overall_gain =
            100.0 * (1.0 - (double)total_compressed_bytes / total_orig_bytes);
        printf("Total original bytes:   %zu\n", total_orig_bytes);
        printf("Total compressed bytes: %zu\n", total_compressed_bytes);
        printf("Overall gain: %.2f%%\n", overall_gain);
    }

    free(data);
    return 0;
}
