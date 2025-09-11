// files/compression/decompress_body.c

#include "decompress_body.h"
#include "bit_reader.h"
#include "code_classes.h"
#include "decoder_map.h"
#include "decompress_header.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define BODY_BUFFER_SIZE 4096

// externs from header
extern uint8_t global_class2_bits;
extern uint8_t global_rle_bits;
extern DecoderMap decoder_map; // assumed present elsewhere

void read_body_using_decoder_map(BitReader *reader, const char *decompress_file_name) {
    FILE *output_file = fopen(decompress_file_name, "wb");
    if (!output_file) {
        fprintf(stderr, "Failed to open output file: %s\n", decompress_file_name);
        exit(EXIT_FAILURE);
    }

    size_t total_bytes_written = 0;

#ifdef DEBUG
    printf("\n=== STARTING BODY DECOMPRESSION ===\n");
    printf("Output file: %s\n", decompress_file_name);
    printf("RLE bits: %u\n", global_rle_bits);
    bitreader_print_state(reader);
#endif

    while (1) {
        uint32_t prefix;
        if (!bitreader_read(reader, &prefix, 1)) {
            // Graceful end: no more bits available
            break;
        }

#ifdef DEBUG
        printf("\n[READ] Prefix bit: %u\n", prefix);
        bitreader_print_state(reader);
#endif

        if (prefix == 0) {
            /* Uncompressed raw byte */
            uint32_t byte;
            if (!bitreader_read(reader, &byte, 8)) {
                // If no more data, treat as normal EOF (padding)
                break;
            }
            fputc((uint8_t)byte, output_file);
            total_bytes_written++;

#ifdef DEBUG
            printf("[RAW] Wrote byte %02X\n", (uint8_t)byte);
            bitreader_print_state(reader);
#endif
        } else {
            /* Compressed entry */
            uint32_t code_class;
            if (!bitreader_read(reader, &code_class, 2)) {
                fprintf(stderr, "Truncated stream: expected code_class after prefix\n");
                exit(EXIT_FAILURE);
            }

#ifdef DEBUG
            printf("[COMPRESSED] code_class=%u\n", code_class);
            bitreader_print_state(reader);
#endif

            if (code_class == 3) {
                /* RLE sequence */
                uint32_t rle_flag;
                if (!bitreader_read(reader, &rle_flag, 1)) {
                    fprintf(stderr, "Truncated stream: expected RLE flag\n");
                    exit(EXIT_FAILURE);
                }

#ifdef DEBUG
                printf("[RLE] rle_flag=%u\n", rle_flag);
                bitreader_print_state(reader);
#endif

                if (rle_flag == 1) {
                    /* Uniform RLE */
                    uint32_t rle_count;
                    if (global_rle_bits > 0) {
                        if (!bitreader_read(reader, &rle_count, global_rle_bits)) {
                            fprintf(stderr, "Truncated stream: expected RLE count (%u bits)\n", global_rle_bits);
                            exit(EXIT_FAILURE);
                        }
                    } else {
                        if (!bitreader_read(reader, &rle_count, 8)) {
                            fprintf(stderr, "Truncated stream: expected RLE count (8 bits fallback)\n");
                            exit(EXIT_FAILURE);
                        }
                    }

                    uint32_t pattern_byte;
                    if (!bitreader_read(reader, &pattern_byte, 8)) {
                        fprintf(stderr, "Truncated stream: expected RLE symbol\n");
                        exit(EXIT_FAILURE);
                    }

#ifdef DEBUG
                    printf("[UNIFORM RLE] count=%u symbol=%02X\n", rle_count, pattern_byte);
#endif

                    for (uint32_t rep = 0; rep < rle_count; ++rep) {
                        fputc((uint8_t)pattern_byte, output_file);
                    }
                    total_bytes_written += rle_count;

                } else {
                    /* Arithmetic RLE */
                    uint32_t start_val, end_val;
                    if (!bitreader_read(reader, &start_val, 8) ||
                        !bitreader_read(reader, &end_val, 8)) {
                        fprintf(stderr, "Truncated stream: expected arithmetic RLE start/end\n");
                        exit(EXIT_FAILURE);
                    }

#ifdef DEBUG
                    printf("[ARITH RLE] start=%02X end=%02X\n", start_val, end_val);
#endif

                    if (end_val < start_val) {
                        fprintf(stderr, "Invalid arithmetic RLE range: start=%u end=%u\n",
                                start_val, end_val);
                        exit(EXIT_FAILURE);
                    }

                    for (uint32_t v = start_val; v <= end_val; ++v) {
                        fputc((uint8_t)v, output_file);
                        total_bytes_written++;
                    }
                }

            } else if (code_class == 0 || code_class == 1 || code_class == 2) {
                /* Normal compressed sequence */
                uint8_t bits = get_code_class_size((uint8_t)code_class, global_class2_bits);
                uint32_t code;
                if (!bitreader_read(reader, &code, bits)) {
                    fprintf(stderr, "Truncated stream: expected code (%u bits)\n", bits);
                    exit(EXIT_FAILURE);
                }

#ifdef DEBUG
                printf("[DECODE] class=%u code=%u bits=%u\n", code_class, code, bits);
#endif

                const uint8_t *seq = NULL;
                uint8_t length = 0;
                if (!decoder_map_get(&decoder_map, (uint16_t)code, (uint8_t)code_class, &seq, &length)) {
                    fprintf(stderr, "Failed to resolve code=%u class=%u\n", code, code_class);
                    exit(EXIT_FAILURE);
                }

                if (length > 0) {
                    fwrite(seq, 1, length, output_file);
                    total_bytes_written += length;
                }

            } else {
                fprintf(stderr, "Invalid code_class read: %u\n", code_class);
                exit(EXIT_FAILURE);
            }
        }

#ifdef DEBUG
        printf("[PROGRESS] Total bytes written: %zu\n", total_bytes_written);
        bitreader_print_state(reader);
#endif
    }

#ifdef DEBUG
    printf("\n=== DECOMPRESSION COMPLETE ===\n");
    printf("Total bytes written: %zu\n", total_bytes_written);
    bitreader_print_state(reader);
#endif

    fclose(output_file);
}
