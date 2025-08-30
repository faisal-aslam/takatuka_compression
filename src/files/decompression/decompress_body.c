// decompress_body.c

#include "decompress_body.h"
#include "bit_reader.h"
#include "code_classes.h"
#include "decoder_map.h"
#include "decompress_header.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define BODY_BUFFER_SIZE 4096

// Add external declarations
extern uint8_t global_class2_bits;
extern uint8_t global_rle_bits;

void read_body_using_decoder_map(BitReader *reader, const char *decompress_file_name) {
    FILE *output_file = fopen(decompress_file_name, "wb");
    if (!output_file) {
        fprintf(stderr, "Failed to open output file: %s\n", decompress_file_name);
        exit(EXIT_FAILURE);
    }

    uint8_t output[256]; // Max sequence size
    uint32_t bit;
    size_t total_bytes_written = 0;

#ifdef DEBUG
    printf("\n=== STARTING BODY DECOMPRESSION ===\n");
    printf("Output file: %s\n", decompress_file_name);
    printf("RLE bits: %u\n", global_rle_bits);
    bitreader_print_state(reader);
#endif

    while (bitreader_read(reader, &bit, 1)) {
#ifdef DEBUG
        printf("\n[READ] Prefix bit: %u\n", bit);
        bitreader_print_state(reader);
#endif

        if (bit == 0) {
            // Uncompressed single byte
            uint32_t byte;
            if (!bitreader_read(reader, &byte, 8)) {
                // Accept EOF if the reader has no more data
                if (reader->overflow) {
                    break;
                }
                fprintf(stderr, "Unexpected EOF while reading uncompressed byte\n");
                exit(EXIT_FAILURE);
            }
            fputc((uint8_t)byte, output_file);
            total_bytes_written++;
        } else {
            // Compressed data - read code class to determine type
            uint32_t code_class;
            if (!bitreader_read(reader, &code_class, 2)) {
                fprintf(stderr, "Failed to read code_class\n");
                exit(EXIT_FAILURE);
            }

#ifdef DEBUG
            printf("[COMPRESSED] Read code class: %u\n", code_class);
            bitreader_print_state(reader);
#endif

            if (code_class == 3) { // RLE
                uint32_t rle_count;

                // Use dynamic RLE bits instead of fixed 8 bits
                if (global_rle_bits > 0) {
                    if (!bitreader_read(reader, &rle_count, global_rle_bits)) {
                        fprintf(stderr, "Failed to read RLE count (%u bits)\n", global_rle_bits);
                        exit(EXIT_FAILURE);
                    }
                } else {
                    // Fallback: should only happen if there are no RLE sequences
                    if (!bitreader_read(reader, &rle_count, 8)) {
                        fprintf(stderr, "Failed to read RLE count (8 bits fallback)\n");
                        exit(EXIT_FAILURE);
                    }
                }

                // Read the RLE pattern byte (always 8 bits)
                uint32_t pattern_byte;
                if (!bitreader_read(reader, &pattern_byte, 8)) {
                    fprintf(stderr, "Failed to read RLE pattern byte\n");
                    exit(EXIT_FAILURE);
                }

                // For single-byte RLE pattern (current implementation)
                uint8_t output_byte = (uint8_t)pattern_byte;

                // Write the repeated byte
                for (uint32_t rep = 0; rep < rle_count; ++rep) {
                    fputc(output_byte, output_file);
                }
                total_bytes_written += rle_count;

#ifdef DEBUG
                printf("[RLE] Count=%u (using %u bits), Pattern=%02X, Total bytes=%zu\n", rle_count, global_rle_bits,
                       output_byte, total_bytes_written);
#endif
            } else if (code_class == 0 || code_class == 1 || code_class == 2) {
                // Regular compressed case (class 0,1 or 2)
                uint8_t bits = get_code_class_size((uint8_t)code_class, global_class2_bits);

                uint32_t code;
                if (!bitreader_read(reader, &code, bits)) {
                    fprintf(stderr, "Failed to read code (%u bits)\n", bits);
                    exit(EXIT_FAILURE);
                }

                const uint8_t *seq = NULL;
                uint8_t length = 0;
                if (!decoder_map_get(&decoder_map, (uint16_t)code, (uint8_t)code_class, &seq, &length)) {
                    fprintf(stderr, "Failed to decode sequence for code=0x%X class=%u\n", code, code_class);
                    exit(EXIT_FAILURE);
                }

                fwrite(seq, 1, length, output_file);
                total_bytes_written += length;
            } else {
                fprintf(stderr, "Invalid code class: %u\n", code_class);
                exit(EXIT_FAILURE);
            }
        }

#ifdef DEBUG
        printf("[PROGRESS] Total bytes written so far: %zu\n", total_bytes_written);
        bitreader_print_state(reader);
#endif
    }

#ifdef DEBUG
    printf("\n=== DECOMPRESSION COMPLETE ===\n");
    printf("Total bytes written: %zu\n", total_bytes_written);
    printf("Final reader state:\n");
    bitreader_print_state(reader);
#endif

    fclose(output_file);
}