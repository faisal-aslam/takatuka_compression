//src/interative_rel/main.c

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../map/seq_freq_map.h"

/// Apply circular shift of array `data` by `shift` positions.
/// Positive shift = move right, Negative shift = move left.
void circular_shift(uint8_t *data, size_t n, int shift) {
    if (n == 0) return;
    shift = ((shift % (int)n) + n) % n; // normalize
    if (shift == 0) return;

    uint8_t *tmp = malloc(n);
    if (!tmp) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    for (size_t i = 0; i < n; i++) {
        tmp[(i + shift) % n] = data[i];
    }

    memcpy(data, tmp, n);
    free(tmp);
}

/// Compress using block RLE
/// Returns compressed size in bytes
size_t rle_compress(const uint8_t *input, size_t n, uint8_t *output) {
    size_t out_pos = 0;
    size_t i = 0;

    while (i < n) {
        // Check for run
        size_t run_len = 1;
        while (i + run_len < n && input[i + run_len] == input[i] && run_len < 255) {
            run_len++;
        }

        if (run_len >= 3) { // encode as run
            output[out_pos++] = 1;           // flag = run
            output[out_pos++] = input[i];    // symbol
            output[out_pos++] = (uint8_t)run_len;
            i += run_len;
        } else {
            // literal block
            size_t lit_start = i;
            size_t lit_len = 0;
            while (i < n && lit_len < 255) {
                // stop if we see a run coming
                if (i + 2 < n && input[i] == input[i+1] && input[i] == input[i+2])
                    break;
                i++;
                lit_len++;
            }
            output[out_pos++] = 0;            // flag = literal
            output[out_pos++] = (uint8_t)lit_len;
            memcpy(&output[out_pos], &input[lit_start], lit_len);
            out_pos += lit_len;
        }
    }
    return out_pos;
}

/// Decompress RLE
size_t rle_decompress(const uint8_t *input, size_t n, uint8_t *output) {
    size_t out_pos = 0;
    size_t i = 0;

    while (i < n) {
        uint8_t flag = input[i++];
        if (flag == 1) {
            uint8_t sym = input[i++];
            uint8_t count = input[i++];
            memset(&output[out_pos], sym, count);
            out_pos += count;
        } else {
            uint8_t len = input[i++];
            memcpy(&output[out_pos], &input[i], len);
            i += len;
            out_pos += len;
        }
    }
    return out_pos;
}

void calculate_frequences(const uint8_t* block, uint32_t size_of_data) {
    init_seq_freq_map();// initialized it.
    for (uint8_t i=0; i < size_of_data; i++) {
        seq_freq_increment(&block[i], 1, 0);
    }
    seq_freq_map_print();
}
int main() {
    // Example input
    uint8_t data[] = "AAAHELLOOOOOOOOOOOOOBBBBYE";
    size_t n = strlen((char*)data);

    //step 1: Add each byte of data in map and count frequencies of each byte.
    calculate_frequences(data, n);
    abort();

    // Try circular shift
    circular_shift(data, n, 5);

    // Compress
    uint8_t *compressed = malloc(2*n); // allocate enough space
    size_t comp_size = rle_compress(data, n, compressed);

    // Decompress
    uint8_t *decompressed = malloc(n+1);
    size_t decomp_size = rle_decompress(compressed, comp_size, decompressed);
    decompressed[decomp_size] = '\0';

    printf("Original size: %zu\n", n);
    printf("Compressed size: %zu\n", comp_size);
    printf("Decompressed: %s\n", decompressed);

    free(compressed);
    free(decompressed);
    return 0;
}
