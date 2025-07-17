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

    uint8_t output[256]; // Max sequence size

    while (1) {
        uint32_t bit;
        if (!bitreader_read(reader, &bit, 1)) {
#ifdef DEBUG
            printf("[EOF] End of stream reached\n");
#endif
            break;
        }

#ifdef DEBUG
        printf("[Prefix Bit] bit = %u\n", bit);
#endif

        if (bit == 0) {
            // Uncompressed single byte
            uint32_t byte;
            if (!bitreader_read(reader, &byte, 8)) {
                fprintf(stderr, "Unexpected EOF while reading uncompressed byte\n");
                exit(EXIT_FAILURE);
            }
#ifdef DEBUG
            printf("[Uncompressed] byte = 0x%02X ('%c')\n", byte, (byte >= 32 && byte <= 126) ? byte : '.');
#endif
            fputc(byte, output_file);
        } else {
            // Compressed or RLE
            uint32_t next_bits;
            if (!bitreader_peek(reader, &next_bits, 3)) {
                fprintf(stderr, "Failed to peek RLE bits\n");
                exit(EXIT_FAILURE);
            }

#ifdef DEBUG
            printf("[Peek] next 3 bits = %u\n", next_bits);
#endif

            if (next_bits <= 8) {
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
                printf("[RLE] length = %u, count = %u\n", rle_len, rle_count);
#endif

                for (uint32_t i = 0; i < rle_len; ++i) {
                    uint32_t temp;
                    if (!bitreader_read(reader, &temp, 8)) {
                        fprintf(stderr, "Failed to read RLE sequence byte\n");
                        exit(EXIT_FAILURE);
                    }
                    output[i] = (uint8_t)temp;
#ifdef DEBUG
                    printf("  [RLE Byte] output[%u] = 0x%02X ('%c')\n", i, output[i], (output[i] >= 32 && output[i] <= 126) ? output[i] : '.');
#endif
                }

                for (uint32_t rep = 0; rep < rle_count; ++rep) {
                    fwrite(output, 1, rle_len, output_file);
#ifdef DEBUG
                    printf("  [RLE Write] Repetition %u of %u\n", rep + 1, rle_count);
#endif
                }
            } else {
                // Normal compressed
                uint32_t code_class;
                if (!bitreader_read(reader, &code_class, 2)) {
                    fprintf(stderr, "Failed to read code_class\n");
                    exit(EXIT_FAILURE);
                }

                uint8_t bits = get_code_class_size(code_class);
                uint32_t code;
                if (!bitreader_read(reader, &code, bits)) {
                    fprintf(stderr, "Failed to read code (%u bits)\n", bits);
                    exit(EXIT_FAILURE);
                }

#ifdef DEBUG
                printf("[Compressed] code_class = %u, bits = %u, code = 0x%X\n", code_class, bits, code);
#endif

                const uint8_t* seq = NULL;
                uint8_t length = 0;
                bool found = decoder_map_get(&decoder_map, code, code_class, &seq, &length);

                if (!found || !seq) {
                    fprintf(stderr, "Failed to decode sequence for code=0x%X class=%u\n", code, code_class);
                    exit(EXIT_FAILURE);
                }

#ifdef DEBUG
                printf("  [Decoded Seq] Length = %u, Bytes = ", length);
                for (uint8_t i = 0; i < length; ++i) {
                    printf("0x%02X ", seq[i]);
                }
                printf("\n");
#endif

                fwrite(seq, 1, length, output_file);
            }
        }
    }

    fclose(output_file);
#ifdef DEBUG
    printf("[Done] Decompression completed and written to: %s\n", decompress_file_name);
#endif
}
