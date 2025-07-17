// decompress_body.c
#include "decompress_body.h"
#include "decompress_header.h"
#include "bit_reader.h"
#include "decoder_map.h"
#include "code_classes.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define BODY_BUFFER_SIZE 4096

void read_body_using_decoder_map(BitReader* reader, const char* decompress_file_name) {
    FILE* output_file = fopen(decompress_file_name, "wb");
    if (!output_file) {
        fprintf(stderr, "Failed to open output file: %s\n", decompress_file_name);
        exit(EXIT_FAILURE);
    }

    uint8_t output[256]; // max sequence size we ever write at once

    while (1) {
        uint32_t bit;
        if (!bitreader_read(reader, &bit, 1)) {
            break; // end of stream
        }

        if (bit == 0) {
            // Uncompressed single byte
            uint32_t byte;
            if (!bitreader_read(reader, &byte, 8)) {
                fprintf(stderr, "Unexpected EOF while reading uncompressed byte\n");
                exit(EXIT_FAILURE);
            }
            fputc(byte, output_file);
        } else {
            // Prefix 1: could be RLE or normal compressed

            // Peek next 3 bits to check if it is RLE (we assume RLE sequences are always < 8 bits long)
            uint32_t next_bits;
            if (!bitreader_peek(reader, &next_bits, 3)) {
                fprintf(stderr, "Failed to peek RLE bits\n");
                exit(EXIT_FAILURE);
            }

            if (next_bits <= 8) {
                // RLE case
                uint32_t rle_len;
                if (!bitreader_read(reader, &rle_len, 3)) {
                    fprintf(stderr, "Failed to read repeat_seq_length\n");
                    exit(EXIT_FAILURE);
                }

                uint32_t rle_count;
                if (!bitreader_read(reader, &rle_count, 8)) {
                    fprintf(stderr, "Failed to read RLE count\n");
                    exit(EXIT_FAILURE);
                }

                for (uint32_t i = 0; i < rle_len; ++i) {
                    uint32_t temp;
                    if (!bitreader_read(reader, &temp, 8)) {
                        fprintf(stderr, "Failed to read RLE sequence byte\n");
                        exit(EXIT_FAILURE);
                    }
                    output[i] = (uint8_t)temp;
                }

                for (uint32_t rep = 0; rep < rle_count; ++rep) {
                    fwrite(output, 1, rle_len, output_file);
                }
            } else {
                // Normal compressed case
                uint32_t code_class;
                if (!bitreader_read(reader, &code_class, 2)) {
                    fprintf(stderr, "Failed to read code_class\n");
                    exit(EXIT_FAILURE);
                }

                uint8_t bits = get_code_class_size(code_class);
                uint32_t code;
                if (!bitreader_read(reader, &code, bits)) {
                    fprintf(stderr, "Failed to read code\n");
                    exit(EXIT_FAILURE);
                }
                const uint8_t* seq = NULL;
                uint8_t length = 0;
                bool found = decoder_map_get(&decoder_map, code, code_class, &seq, &length);

                uint8_t len = get_code_class_size(code_class);

                if (!seq) {
                    fprintf(stderr, "Failed to decode sequence for code=0x%X class=%u\n", code, code_class);
                    exit(EXIT_FAILURE);
                }
                fwrite(seq, 1, len, output_file);
            }
        }
    }

    fclose(output_file);
}