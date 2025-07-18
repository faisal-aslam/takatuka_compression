#include "decompress_body.h"
#include "bit_reader.h"
#include "code_classes.h"
#include "decoder_map.h"
#include "decompress_header.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define BODY_BUFFER_SIZE 4096

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
                if (reader->bit_pos == 0 && reader->byte_pos >= reader->buffer_size) {
                    // Graceful EOF: don't throw error
                    break;
                }
                fprintf(stderr, "Unexpected EOF while reading uncompressed byte\n");
                exit(EXIT_FAILURE);
            }

#ifdef DEBUG
            printf("[UNCOMPRESSED] Byte: 0x%02X (%c)\n", byte, (byte >= 32 && byte <= 126) ? byte : '.');
            printf("  Writing 1 raw byte to output\n");
#endif

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

            if (code_class == 3) { // 0b11 indicates RLE
                // RLE case
                uint32_t rle_len;
                if (!bitreader_read(reader, &rle_len, 3)) {
                    fprintf(stderr, "Failed to read RLE length\n");
                    exit(EXIT_FAILURE);
                }

                uint32_t rle_count;
                if (!bitreader_read(reader, &rle_count, 8)) {
                    fprintf(stderr, "Failed to read RLE count\n");
                    exit(EXIT_FAILURE);
                }

#ifdef DEBUG
                printf("[RLE] Pattern length: %u, Repeat count: %u\n", rle_len, rle_count);
                printf("  Reading pattern bytes:\n");
#endif

                // Read the RLE pattern bytes
                for (uint32_t i = 0; i < rle_len; ++i) {
                    uint32_t temp;
                    if (!bitreader_read(reader, &temp, 8)) {
                        fprintf(stderr, "Failed to read RLE sequence byte\n");
                        exit(EXIT_FAILURE);
                    }
                    output[i] = (uint8_t)temp;
#ifdef DEBUG
                    printf("    [RLE BYTE %u] 0x%02X (%c)\n", i, output[i],
                           (output[i] >= 32 && output[i] <= 126) ? output[i] : '.');
#endif
                }

                // Write the repeated sequence
#ifdef DEBUG
                printf("  Writing %u repetitions of %u-byte pattern:\n", rle_count, rle_len);
                for (uint32_t i = 0; i < rle_len; i++) {
                    printf("    0x%02X ", output[i]);
                }
                printf("\n");
#endif

                for (uint32_t rep = 0; rep < rle_count; ++rep) {
                    fwrite(output, 1, rle_len, output_file);
#ifdef DEBUG
                    printf("    [REP %u/%u] Written\n", rep + 1, rle_count);
#endif
                }
                total_bytes_written += rle_len * rle_count;
            } else {
                // Regular compressed case (code_class 0, 1, or 2)
                uint8_t bits = get_code_class_size(code_class);
                uint32_t code;
                if (!bitreader_read(reader, &code, bits)) {
                    fprintf(stderr, "Failed to read code (%u bits)\n", bits);
                    exit(EXIT_FAILURE);
                }

#ifdef DEBUG
                printf("[COMPRESSED] Code bits: %u, Code value: %u\n", bits, code);
                bitreader_print_state(reader);
                printf("  Looking up in decoder map...\n");
#endif

                const uint8_t *seq = NULL;
                uint8_t length = 0;
                if (!decoder_map_get(&decoder_map, code, code_class, &seq, &length)) {
                    fprintf(stderr, "Failed to decode sequence for code=0x%X class=%u\n", code, code_class);
                    exit(EXIT_FAILURE);
                }

#ifdef DEBUG
                printf("  DECODED SEQUENCE: Length=%u, Bytes: ", length);
                for (uint8_t i = 0; i < length; i++) {
                    printf("0x%02X ", seq[i]);
                }
                printf("\n  Writing to output\n");
#endif

                fwrite(seq, 1, length, output_file);
                total_bytes_written += length;
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