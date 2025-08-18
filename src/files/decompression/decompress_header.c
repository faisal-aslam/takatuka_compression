// decompress_header.c
//
// Decoder for the compact header format produced by the updated compressor.
// Header layout (bit-packed, no unnecessary padding):
//  - 16 bits : total number of codewords (N)
//  - 8  bits : class2_bits (B2)      -- present iff N > 0
//  - 8  bits : len_bits[0]           -- number of bits used to encode (length-1) for class 0 entries
//  - 8  bits : len_bits[1]
//  - 8  bits : len_bits[2]
//  - Then entries in class order: first all Class0 entries, then Class1, then Class2.
//    For each entry (index order within class):
//      - if len_bits[class] > 0 : read length_minus_one using len_bits[class] bits and decode length = length_minus_one + 1
//      - else (len_bits[class] == 0): length = 1 (no bits stored)
//      - read `length` bytes (8 bits each) for the sequence
//
// Notes:
//  * No per-entry class or index fields are stored — index is the ordinal position inside that class.
//  * The decoder derives the number of entries per class from N and class capacities:
//      n0 = min(N, C0)
//      n1 = min(max(N - n0, 0), C1)
//      n2 = N - n0 - n1
//    where C0 = get_code_class_threshold(0, 0) (fixed), C1 likewise, and C2 = get_code_class_threshold(2, B2).
//  * The only byte boundary alignment performed is a final call to bitreader_move_byte_boundary()
//    after header parsing so the body reader can start aligned if necessary.  All header fields are
//    read bit-packed — this keeps the header minimal.

#include "decompress_header.h"
#include "bit_reader.h"
#include "decoder_map.h"
#include "code_classes.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>

/* Global decoder map and the class2 bit-width for this file (exported in header) */
DecoderMap decoder_map;
uint8_t global_class2_bits = 0; /* set by read_header_and_create_decoder_map() */

/* Macro: attempt to read `bits` bits; if bitreader_read fails, try to refill buffer and retry.
   On persistent failure, print message and exit. */
#define SAFE_BITREAD(reader_ptr, out_var, bits)                                         \
    do {                                                                                 \
        if (!bitreader_read((reader_ptr), (out_var), (bits))) {                          \
            /* try refill and retry once */                                              \
            if (!bitreader_fill_next_chunk((reader_ptr))) {                              \
                fprintf(stderr, "Failed to read %u bits (EOF/overflow)\\n", (unsigned)(bits)); \
                exit(EXIT_FAILURE);                                                       \
            }                                                                            \
            if (!bitreader_read((reader_ptr), (out_var), (bits))) {                      \
                fprintf(stderr, "bitreader_read failed after refill for %u bits\\n", (unsigned)(bits)); \
                exit(EXIT_FAILURE);                                                       \
            }                                                                            \
        }                                                                                \
    } while (0)


/* Helper to derive per-class counts from total N using the encoder's greedy policy. */
static void derive_class_counts(uint16_t N, uint16_t cap0, uint16_t cap1, uint16_t cap2,
                                uint16_t out_counts[3]) {
    uint16_t n0 = (N < cap0) ? N : cap0;
    uint16_t rem = (N > n0) ? (N - n0) : 0;
    uint16_t n1 = (rem < cap1) ? rem : cap1;
    uint16_t n2 = (N > (n0 + n1)) ? (N - n0 - n1) : 0;
    out_counts[0] = n0;
    out_counts[1] = n1;
    out_counts[2] = n2;
}

/* Main header reader */
void read_header_and_create_decoder_map(BitReader* reader) {
    if (!reader) {
        fprintf(stderr, "read_header_and_create_decoder_map: reader == NULL\n");
        exit(EXIT_FAILURE);
    }

    uint32_t tmp_n = 0;
    SAFE_BITREAD(reader, &tmp_n, 16);
    if (tmp_n >= 0x10000u) {
        fprintf(stderr, "Header: invalid num_codes >= 2^16: %u\n", tmp_n);
        exit(EXIT_FAILURE);
    }
    uint16_t num_codes = (uint16_t)tmp_n;

#ifdef DEBUG
    printf("[DEBUG] Read num_codes = %u\n", (unsigned)num_codes);
    bitreader_print_state(reader);
#endif

    /* Initialize decoder map with capacity for num_codes */
    init_decoder_map(&decoder_map, num_codes);

    /* If no codes, encoder wrote only the 16-bit placeholder and nothing else */
    if (num_codes == 0) {
        global_class2_bits = 0;
#ifdef DEBUG
        printf("[DEBUG] No codes in header; set global_class2_bits = 0\n");
#endif
        /* Align to next byte boundary so body reader starts clean (encoder's writer may have byte_tail) */
        bitreader_move_byte_boundary(reader);
        return;
    }

    /* Read class2_bits (one byte) */
    uint32_t cb = 0;
    SAFE_BITREAD(reader, &cb, 8);
    global_class2_bits = (uint8_t)cb;

#ifdef DEBUG
    printf("[DEBUG] Read class2_bits = %u\n", (unsigned)global_class2_bits);
    bitreader_print_state(reader);
#endif

    /* Read per-class len_bits (one byte each) */
    uint32_t lb0 = 0, lb1 = 0, lb2 = 0;
    SAFE_BITREAD(reader, &lb0, 8);
    SAFE_BITREAD(reader, &lb1, 8);
    SAFE_BITREAD(reader, &lb2, 8);

    uint8_t len_bits[3];
    len_bits[0] = (uint8_t)lb0;
    len_bits[1] = (uint8_t)lb1;
    len_bits[2] = (uint8_t)lb2;

#ifdef DEBUG
    printf("[DEBUG] Read len_bits = [%u, %u, %u]\n", (unsigned)len_bits[0], (unsigned)len_bits[1], (unsigned)len_bits[2]);
    bitreader_print_state(reader);
#endif

    /* Derive per-class capacities and counts */
    uint16_t cap0 = get_code_class_threshold(0, 0); /* fixed */
    uint16_t cap1 = get_code_class_threshold(1, 0); /* fixed */
    uint16_t cap2 = get_code_class_threshold(2, global_class2_bits); /* dynamic */

    uint16_t counts[3];
    derive_class_counts(num_codes, cap0, cap1, cap2, counts);

    if (counts[2] > cap2) {
        fprintf(stderr, "Header error: class2 needs %u entries but capacity is %u (class2_bits=%u)\n",
                (unsigned)counts[2], (unsigned)cap2, (unsigned)global_class2_bits);
        exit(EXIT_FAILURE);
    }

#ifdef DEBUG
    printf("[DEBUG] Derived class counts: n0=%u, n1=%u, n2=%u (caps: %u,%u,%u)\n",
           (unsigned)counts[0], (unsigned)counts[1], (unsigned)counts[2],
           (unsigned)cap0, (unsigned)cap1, (unsigned)cap2);
#endif

    /* Read entries in class order (0,1,2); index within class = ordinal position */
    for (int cls = 0; cls <= 2; ++cls) {
        uint16_t n_here = counts[cls];
        uint8_t lb = len_bits[cls];

        for (uint16_t idx = 0; idx < n_here; ++idx) {
            uint32_t length = 0;

            if (lb > 0) {
                uint32_t lminus;
                SAFE_BITREAD(reader, &lminus, lb);
                /* lminus is length-1 */
                length = lminus + 1;
            } else {
                /* len_bits == 0 => encoder omitted length field; that implies length == 1 */
                length = 1;
            }

            if (length == 0) {
                fprintf(stderr, "Header error: decoded zero length for class %d index %u\n", cls, (unsigned)idx);
                exit(EXIT_FAILURE);
            }

            /* allocate sequence buffer */
            uint8_t *seq = (uint8_t*)malloc((size_t)length);
            if (!seq) {
                fprintf(stderr, "OOM allocating %u bytes for sequence (class %d index %u)\n", (unsigned)length, cls, (unsigned)idx);
                exit(EXIT_FAILURE);
            }

            /* read sequence bytes (each 8 bits) */
            for (uint32_t b = 0; b < length; ++b) {
                uint32_t bv;
                SAFE_BITREAD(reader, &bv, 8);
                seq[b] = (uint8_t)bv;
            }

            /* store mapping: index-within-class = idx, class = cls */
            /* Keep parameter order same as existing decoder_map_set usage */
            if (!decoder_map_set(&decoder_map, (uint16_t)idx, (uint8_t)cls, seq, (uint8_t)length)) {
                fprintf(stderr, "decoder_map_set failed for class=%u idx=%u\n", (unsigned)cls, (unsigned)idx);
                exit(EXIT_FAILURE);
            }
            /* decoder_map_set assumed to take ownership of seq */
        }
    }

#ifdef DEBUG
    printf("[DEBUG] Completed DecoderMap reconstruction (num_codes=%u)\n", (unsigned)num_codes);
    print_decoder_map(&decoder_map);
#endif

    /* Align to next byte boundary so the body reader starts at a byte boundary (if needed) */
    bitreader_move_byte_boundary(reader);
}
