#include "sort.h"
#include "bitmap.h"
#include "suitable_sequences.h"
#include "fileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>   // ceil, log2

#define BLOCK_SIZE 256
#define BIT_MAP_THREASHOLD (BLOCK_SIZE*8/2)
#define COMPRESSION_THRESHOLD 1.0
#define MIN_SUBBLOCK_SIZE 10
#define DENSITY 1.0   // tune for suitable_sequences()

extern SortInfo sort_info;

/* Estimate compressed size (in bytes) for a sorted block:
   encoding counts for each symbol in the value range, using
   ceil(log2(max_run + 1)) bits per count, plus 1 byte overhead. */
static double sorted_data_compression(const uint8_t *block, int n) {
    if (n <= 0) return 0.0;

    int max_repeated = 1;
    int current_repeated = 1;

    /* Step 1: validate sorted & compute max run length */
    for (int i = 1; i < n; i++) {
        if (block[i] == block[i - 1]) {
            current_repeated++;
            if (current_repeated > max_repeated) {
                max_repeated = current_repeated;
            }
        } else {
            current_repeated = 1;
            if (block[i] < block[i - 1]) { // not sorted
                fprintf(stderr, "sorted_data_compression: illegal data (not sorted)\n");
                abort();
            }
        }
    }

    /* Step 2: number of unique values = range size */
    int symbol_range = (int)block[n - 1] - (int)block[0] + 1;
    if (symbol_range <= 0) symbol_range = 1;

    /* Step 3: bits needed to store counts up to max_repeated */
    int bits_per_count = 1;
    if (max_repeated > 1) {
        /* bits to represent values from 0..max_repeated (inclusive) */
        bits_per_count = (int)ceil(log2((double)max_repeated + 1.0));
        if (bits_per_count <= 0) bits_per_count = 1;
    }

    /* Step 4: total bits = symbol_range * bits_per_count */
    double total_bits = (double)symbol_range * (double)bits_per_count;

    /* Step 5: convert to bytes + 1 byte overhead for base symbol */
    double total_bytes = ceil(total_bits / 8.0) + 1.0;

    return total_bytes;
}

// -------------------- Debug printing --------------------
static void print_block(const uint8_t *data, int n) {
    for (int i = 0; i < n; i++) {
        printf("%3u ", (unsigned)data[i]);
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
    double total_compressed_bytes = 0.0; /* double because compressed estimates can be fractional */

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
        if (sub_len <= 0) {
            printf("Block %zu: find_suitable_subblock returned invalid range [%d..%d]\n\n",
                   i / BLOCK_SIZE, best_start, best_end);
            continue;
        }

        printf("Block %zu: found subblock [%d..%d], len=%d\n",
               i / BLOCK_SIZE, best_start, best_end, sub_len);

        /* Copy subblock and sort it */
        uint8_t *subblock = malloc((size_t)sub_len);
        if (!subblock) {
            fprintf(stderr, "malloc failed\n");
            free(data);
            return 1;
        }
        memcpy(subblock, block + best_start, (size_t)sub_len);

        sort_info.original_size = sub_len;
        sort_info.bitmap_size = 0;
        merge_sort(subblock, sub_len, sub_len);

        /* Compress sorted subblock (estimated bytes) */
        double compressed_size = sorted_data_compression(subblock, sub_len);

        /* Include bitmap size (assumed to be in bytes). If bitmap_size is bits, divide by 8. */
        double effective_size = compressed_size + (double)sort_info.bitmap_size/8.0;

        double gain = 100.0 * (1.0 - effective_size / (double)sub_len);

        printf("  orig=%d, compressed(est)=%.2f, bitmap=%zu, effective=%.2f, gain=%.2f%%\n",
               sub_len, compressed_size, sort_info.bitmap_size, effective_size, gain);

        total_orig_bytes += (size_t)sub_len;
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
            100.0 * (1.0 - total_compressed_bytes / (double)total_orig_bytes);
        printf("Total original bytes:   %zu\n", total_orig_bytes);
        printf("Total compressed bytes: %.2f\n", total_compressed_bytes);
        printf("Overall gain: %.2f%%\n", overall_gain);
    }

    free(data);
    return 0;
}
