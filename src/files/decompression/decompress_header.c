// decompress_header.c

#include "decompress_header.h"
#include "bit_reader.h"
#include "decoder_map.h"
#include "code_classes.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

DecoderMap decoder_map;

// Macro to safely read bits, refilling buffer from file if needed
#define SAFE_BITREAD(reader_ptr, out, bits)                              \
    do {                                                                 \
        if (!bitreader_read(reader_ptr, out, bits)) {                    \
            if (!bitreader_fill_next_chunk(reader_ptr)) {                \
                fprintf(stderr, "Failed to read %u bits (EOF/overflow)\n", bits); \
                exit(EXIT_FAILURE);                                      \
            }                                                            \
            if (!bitreader_read(reader_ptr, out, bits)) {                \
                fprintf(stderr, "bitreader_read failed again\n");        \
                exit(EXIT_FAILURE);                                      \
            }                                                            \
        }                                                                \
    } while (0)

void read_header_and_create_decoder_map(BitReader* reader) {
    uint32_t num_codes;
    SAFE_BITREAD(reader, &num_codes, 16);

#ifdef DEBUG
    printf("[DEBUG] ⏎ Read 16 bits: num_codes = %u\n", num_codes);
    bitreader_print_state(reader);
#endif

    init_decoder_map(&decoder_map, num_codes);

    for (uint32_t i = 0; i < num_codes; ++i) {
        uint32_t code_class, length, code_index;

        SAFE_BITREAD(reader, &code_class, 2);
#ifdef DEBUG
        printf("[DEBUG] ⏎ Read 2 bits: code_class = %u\n", code_class);
        bitreader_print_state(reader);
#endif

        SAFE_BITREAD(reader, &length, 8);
#ifdef DEBUG
        printf("[DEBUG] ⏎ Read 8 bits: sequence_length = %u\n", length);
        bitreader_print_state(reader);
#endif

        uint8_t class_bits = get_code_class_size((uint8_t)code_class);
        SAFE_BITREAD(reader, &code_index, class_bits);
#ifdef DEBUG
        printf("[DEBUG] ⏎ Read %u bits: code_index = %u (class %u)\n", class_bits, code_index, code_class);
        bitreader_print_state(reader);
#endif

        uint8_t* sequence = malloc(length);
        if (!sequence) {
            fprintf(stderr, "Memory allocation failure for sequence\n");
            exit(EXIT_FAILURE);
        }

        for (uint8_t j = 0; j < length; ++j) {
            uint32_t byte_val;
            SAFE_BITREAD(reader, &byte_val, 8);
            sequence[j] = (uint8_t)byte_val;
#ifdef DEBUG
            printf("[DEBUG] ⏎ Read byte #%u: %02X\n", j, sequence[j]);
            bitreader_print_state(reader);
#endif
        }

        decoder_map_set(&decoder_map, code_index, (uint8_t)code_class, sequence, (uint8_t)length);
        // Do not free sequence — it's owned by DecoderMap
    }

#ifdef DEBUG
    printf("[DEBUG] Completed DecoderMap reconstruction.\n");
    print_decoder_map(&decoder_map);
#endif
    bitreader_move_byte_boundary(reader);
}