// rle_main.c
//
// Test driver for block-based reversible merge-sort with bitmap + compression.
// Now includes two compression estimators:
//   1. Delta + RLE with varint encoding
//   2. Delta + RLE with Rice coding (k = 3)
//
// The goal: check if we can save > COMPRESSION_THRESHOLD% compared
// to the raw block size. If so, accept the block.
//

#include "sort.h"
#include "bitmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define BLOCK_SIZE 32
#define BIT_MAP_THREASHOLD (32*8/2)
#define COMPRESSION_THRESHOLD 50.0  // % compression gain required

extern SortInfo sort_info;

static void print_block(const uint8_t *data, int block_size) {
    for (int i = 0; i < block_size; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

// ======================================================================
// Variable-length integer size (7-bit continuation scheme)
// ======================================================================
static int varint_size(unsigned int x) {
    int size = 0;
    do {
        x >>= 7;
        size++;
    } while (x);
    return size;
}

// ======================================================================
// Delta + RLE with varint size estimator
// ----------------------------------------------------------------------
// Encoding scheme:
//   - Store first value (1 byte).
//   - For each run: encode (delta, run_len) using varint coding.
// ======================================================================
static int delta_rle_varint_size(const uint8_t *block, int n) {
    if (n <= 0) return 0;

    int size_bytes = 1; // store first value explicitly
    int prev = block[0];

    for (int i = 1; i < n; ) {
        int run_val = block[i];
        int run_len = 1;

        // Count run length
        while (i + run_len < n && block[i + run_len] == run_val) {
            run_len++;
        }

        int delta = run_val - prev;
        // delta + run_len, both varint-encoded
        size_bytes += varint_size(delta) + varint_size(run_len);

        prev = run_val;
        i += run_len;
    }

    return size_bytes;
}

// ======================================================================
// Delta + RLE with Rice coding (k = 3)
// ----------------------------------------------------------------------
// Rice coding encodes an integer x as:
//   quotient = x >> k   (in unary)
//   remainder = x & ((1<<k)-1)  (in binary, k bits)
// Cost in bits = quotient + 1 (unary terminator) + k
// ======================================================================
static int rice_size(unsigned int x, int k) {
    unsigned int q = x >> k;   // quotient
    unsigned int r = x & ((1U << k) - 1); // remainder
    (void)r; // remainder value not needed, only its bit length = k
    return (q + 1) + k; // total bits used
}

static int delta_rle_rice_size(const uint8_t *block, int n, int k) {
    if (n <= 0) return 0;

    int size_bits = 8; // first value stored in 8 bits
    int prev = block[0];

    for (int i = 1; i < n; ) {
        int run_val = block[i];
        int run_len = 1;

        while (i + run_len < n && block[i + run_len] == run_val) {
            run_len++;
        }

        int delta = run_val - prev;
        if (delta < 0) delta = -delta; // handle signed → Rice needs unsigned

        // Encode delta and run length with Rice coding
        size_bits += rice_size((unsigned)delta, k);
        size_bits += rice_size((unsigned)run_len, k);

        prev = run_val;
        i += run_len;
    }

    // Round up to full bytes
    return (size_bits + 7) / 8;
}

// ======================================================================
// Main
// ======================================================================
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
    printf("Compression threshold = %.1f%%\n", COMPRESSION_THRESHOLD);
    printf("Rice parameter k = 3\n\n");

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

        merge_sort(block, BLOCK_SIZE, BLOCK_SIZE);

        if (sort_info.bitmap_size <= BIT_MAP_THREASHOLD) {
            // Compute compressed sizes
            int varint_size_bytes = delta_rle_varint_size(block, BLOCK_SIZE);
            int rice_size_bytes   = delta_rle_rice_size(block, BLOCK_SIZE, 3);

            // Take the better of the two encodings
            int best_size = (varint_size_bytes < rice_size_bytes)
                            ? varint_size_bytes
                            : rice_size_bytes;

            // Compute overall gain vs raw block size
            double gain = 100.0 * (1.0 - (double)best_size / BLOCK_SIZE);

            if (gain >= COMPRESSION_THRESHOLD) {
                accepted_blocks++;
                printf("=== ACCEPTED (start=%zu, bitmap=%zu, varint=%d, rice=%d, gain=%.2f%%) ===\n",
                       i, sort_info.bitmap_size, varint_size_bytes, rice_size_bytes, gain);
            } else {
                /*printf("=== REJECTED by compression (start=%zu, bitmap=%zu, varint=%d, rice=%d, gain=%.2f%%) ===\n",
                       i, sort_info.bitmap_size, varint_size_bytes, rice_size_bytes, gain);*/
            }

            print_block(data + i, BLOCK_SIZE);
            print_bitmap(sort_info.bitmap, sort_info.bitmap_size);
            printf("\n");

            i += BLOCK_SIZE;  // skip whole block
        } else {
            /*printf("=== IGNORED by bitmap (start=%zu, bitmap=%zu) ===\n",
                   i, sort_info.bitmap_size);

            print_block(data + i, BLOCK_SIZE);
            print_bitmap(sort_info.bitmap, sort_info.bitmap_size);
            printf("\n");*/

            i += 1;  // slide window by one
        }

        free(block);
    }

    double percent = (total_blocks > 0)
                     ? (100.0 * accepted_blocks / total_blocks)
                     : 0.0;

    printf("Summary: accepted %zu / %zu blocks (%.2f%%)\n",
           accepted_blocks, total_blocks, percent);

    free(data);
    return 0;
}
