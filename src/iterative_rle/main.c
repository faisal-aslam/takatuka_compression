// src/interative_rel/main.c

#include "seq_freq_map.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILE_SIZE (60 * 1024) // 60 KB limit

uint32_t distance_saver[256][MAX_FILE_SIZE];
uint32_t sizes[256] = {0};
uint32_t last_actual_position[256] = {0};

#define FREQ_THRESHOLD 10
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

        if (run_len >= 3) {               // encode as run
            output[out_pos++] = 1;        // flag = run
            output[out_pos++] = input[i]; // symbol
            output[out_pos++] = (uint8_t)run_len;
            i += run_len;
        } else {
            // literal block
            size_t lit_start = i;
            size_t lit_len = 0;
            while (i < n && lit_len < 255) {
                // stop if we see a run coming
                if (i + 2 < n && input[i] == input[i + 1] && input[i] == input[i + 2]) break;
                i++;
                lit_len++;
            }
            output[out_pos++] = 0; // flag = literal
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

void calculate_frequences(const uint8_t *block, uint32_t size_of_data) {
    init_seq_freq_map();
    for (uint32_t i = 0; i < size_of_data; i++) {
        seq_freq_increment(&block[i], 1, 0);
    }
    seq_freq_map_print();
}

/// Read file into buffer (up to MAX_FILE_SIZE)
size_t read_file(const char *filename, uint8_t *buffer, size_t max_size) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("fopen");
        exit(1);
    }
    size_t n = fread(buffer, 1, max_size, f);
    if (ferror(f)) {
        perror("fread");
        fclose(f);
        exit(1);
    }
    fclose(f);
    return n;
}

void apply_delta_mod_transform() {
    for (int b = 0; b < 256; b++) {
        if (sizes[b] > 1) {
            for (uint32_t j = sizes[b] - 1; j > 0; j--) {
                // force into signed difference first, then wrap to [0,255]
                int diff = (int)distance_saver[b][j] - (int)distance_saver[b][j - 1];
                distance_saver[b][j] = (uint8_t)((diff % 256 + 256) % 256);
            }
        }
    }
}

/// Open a file for binary writing. If it exists, overwrite it.
/// Returns the FILE* stream, or exits on error.
FILE *open_output_file(const char *filename) {
    FILE *f = fopen(filename, "wb"); // "wb" = write binary, truncate
    if (!f) {
        perror("fopen");
        exit(1);
    }
    return f;
}

/// Append a single byte to an open file stream.
/// Flush is optional — leave to caller to fclose().
void append_byte_to_file(FILE *f, uint8_t data) {
    if (fwrite(&data, 1, 1, f) != 1) {
        perror("fwrite");
        fclose(f);
        exit(1);
    }
}

// Function to perform Insertion Sort
void insertionSort(uint8_t* block, uint32_t size_of_block) {
    int i, key, j;
    uint8_t i;
    for (i = 1; i < size_of_block; i++) {
        key = block[i]; // Store the current element to be inserted
        j = i - 1;   // Start comparing from the element before the current one

        // Move elements of arr[0..i-1], that are greater than key,
        // to one position ahead of their current position
        while (j >= 0) {
            if (block[j] > key) {
                block[j + 1] = block[j];
                j = j - 1;
                //we swap thus record 
            } else {

            }
        }
        block[j + 1] = key; // Place the key in its correct sorted position
    }
}

void print_distances(const char *title) {
    printf("=== %s ===\n", title);
    int less_than_16 = 0;
    int total = 0;
    for (int b = 0; b < 256; b++) {
        if (sizes[b] > 0) {
            /*printf("Byte %3d (0x%02X, '%c'): ", b, b,
                   (b >= 32 && b <= 126) ? b : '.');*/
            for (uint32_t j = 0; j < sizes[b]; j++) {
                if (distance_saver[b][j] < 16) less_than_16++;
                total++;
                printf("%u ", distance_saver[b][j]);
            }
             printf("\n");
        }
    }
    printf("\n\n total=%d, less than 16=%u\n", total, less_than_16);
}

void write_distances_in_file() {
    FILE *file = open_output_file("test.out");
    uint32_t total_byte_written_in_file = 0;
    for (int b = 0; b < 256; b++) {
        if (sizes[b] > 0) {
            /*printf("Byte %3d (0x%02X, '%c'): ", b, b,
                   (b >= 32 && b <= 126) ? b : '.');*/
            for (uint32_t j = 0; j < sizes[b]; j++) {
                printf("%u", distance_saver[b][j]);
                append_byte_to_file(file, (uint8_t)distance_saver[b][j]);
                total_byte_written_in_file++;
            }
            // printf("\n");
        }
    }
    printf("\nTotal bytes written in file=%u\n", total_byte_written_in_file);
    fclose(file);
}

void find_distance(const uint8_t *block, uint32_t block_size) {
    // go through file and calculate distances between same byte of data.
    for (uint32_t i = 0; i < block_size; i++) {
        uint8_t current = block[i];
        if (sizes[current] == 0) {
            distance_saver[current][sizes[current]++] = i;
            last_actual_position[current] = i;
        } else {
            distance_saver[current][sizes[current]++] = i - last_actual_position[current];
            last_actual_position[current] = i;
        }
    }
}

void print_file_bytes(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("fopen");
        exit(1);
    }
    uint16_t greater_than_32 = 0;
    uint16_t total = 0;
    int ch;
    while ((ch = fgetc(f)) != EOF) {
        if (ch > 32) greater_than_32++;
        if (ch > 128) greater_than_32++;
        printf("%u ", (uint8_t)ch);
        total++;
    }

    printf("\n total=%u, greater_than_32=%u\n", total, greater_than_32);
    fclose(f);
}

void print_block(const uint8_t* block, uint32_t size_of_block) {
    for (uint32_t i=0; i < size_of_block; i++) {
        printf("%u ", block[i]);
    }
    printf("\n\n\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    // Read input file
    uint8_t *data = malloc(MAX_FILE_SIZE);
    if (!data) {
        fprintf(stderr, "Out of memory\n");
        return 1;
    }

    size_t n = read_file(argv[1], data, MAX_FILE_SIZE);
    printf("Read %zu bytes from %s\n", n, argv[1]);

    printf("\n****** Printing File bytes ************\n\n");
    print_file_bytes(argv[1]);

    insertionSort(data, n);

    print_block(data, n);
    abort();

    printf("\n\n****** Calculate frequencies ************\n\n");
    // Step 1: Add each byte of data in map and count frequencies
    calculate_frequences(data, n);

    find_distance(data, n);

    print_distances("\n\n\n***** Distances Before delta transformation\n\n");
    

    // write_distances_in_file();
    apply_delta_mod_transform();
    print_distances("\n\n\n***** Distances After delta transformation\n\n");

    abort();
    write_distances_in_file();

    // Try circular shift (example: shift by 5)
    circular_shift(data, n, 5);

    // Compress
    uint8_t *compressed = malloc(2 * n);
    size_t comp_size = rle_compress(data, n, compressed);

    // Decompress
    uint8_t *decompressed = malloc(n);
    size_t decomp_size = rle_decompress(compressed, comp_size, decompressed);

    printf("Original size: %zu\n", n);
    printf("Compressed size: %zu\n", comp_size);
    printf("Decompressed size: %zu\n", decomp_size);

    // Verify
    if (memcmp(data, decompressed, n) == 0) {
        printf("Decompression successful (matches input after shift).\n");
    } else {
        printf("Warning: Decompressed data does not match shifted input.\n");
    }

    free(data);
    free(compressed);
    free(decompressed);
    return 0;
}
