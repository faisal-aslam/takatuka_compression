// decompress_header.c

#include "decompress_header.h"
#include "bit_reader.h"
#include "decoder_map.h"
#include "code_classes.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

/* Global decoder map and the class2 bit-width for this file */
DecoderMap decoder_map;
uint8_t global_class2_bits = 0; /* set by read_header_and_create_decoder_map() */

/* Macro to safely read bits, refilling buffer from file if needed */
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

/*
 * Header layout (as written by your compressor):
 *  - 16 bits : number of codes (num_codes) [written at start as placeholder then overwritten]
 *  - 8 bits  : class2_bits (only present if num_codes > 0)
 *  - repeated num_codes times:
 *      - 2 bits : code_class (00/01/10 normal, 11 = RLE)
 *      - 8 bits : sequence length
 *      - N bits : code index (N depends on class; for class 2 use dynamic class2_bits)
 *      - sequence bytes (length bytes)
 *
 * After reading header entries we must have a DecoderMap that maps (code, class) -> sequence.
 */
void read_header_and_create_decoder_map(BitReader* reader) {
    uint32_t num_codes;
    SAFE_BITREAD(reader, &num_codes, 16);

#ifdef DEBUG
    printf("[DEBUG] ⏎ Read 16 bits: num_codes = %u\n", num_codes);
    bitreader_print_state(reader);
#endif

    /* Initialize decoder map capacity based on num_codes */
    init_decoder_map(&decoder_map, num_codes);

    /* If there are no codes, encoder wrote only the 16-bit zero placeholder, no class2_bits byte */
    if (num_codes == 0) {
        global_class2_bits = 0;
#ifdef DEBUG
        printf("[DEBUG] No codes in header (num_codes == 0). global_class2_bits set to 0.\n");
#endif
        /* align to next byte boundary (encoder also did bitwriter_reset / write) */
        bitreader_move_byte_boundary(reader);
        return;
    }

    /* Read the class2_bits byte that encoder wrote after the 16-bit count */
    uint32_t cbits;
    SAFE_BITREAD(reader, &cbits, 8);
    global_class2_bits = (uint8_t)cbits;

#ifdef DEBUG
    printf("[DEBUG] ⏎ Read 8 bits: class2_bits = %u\n", global_class2_bits);
    bitreader_print_state(reader);
#endif

    /* Read each header entry and populate decoder_map */
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

        /* Use dynamic class2 bit-width when class==2 */
        uint8_t class_bits = get_code_class_size((uint8_t)code_class, global_class2_bits);
        if (class_bits > 0) {
            SAFE_BITREAD(reader, &code_index, class_bits);
        } else {
            /* zero-bit index -> only possible value is 0 */
            code_index = 0;
        }
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

        /* Store mapping (code_index, class) -> sequence in decoder map */
        if (!decoder_map_set(&decoder_map, (uint16_t)code_index, (uint8_t)code_class, sequence, (uint8_t)length)) {
            /* This should be rare; indicates load factor or capacity error */
            fprintf(stderr, "decoder_map_set failed for code=%u class=%u (i=%u)\n", code_index, code_class, i);
            exit(EXIT_FAILURE);
        }
        /* sequence is now owned by decoder_map */
    }

#ifdef DEBUG
    printf("[DEBUG] Completed DecoderMap reconstruction.\n");
    print_decoder_map(&decoder_map);
#endif

    /* Align to next byte boundary (encoder also aligned) */
    bitreader_move_byte_boundary(reader);
}
