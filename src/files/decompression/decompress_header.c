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
//      - if len_bits[cla// decompress_header.c
//
// Header layout description unchanged (see previous comments).
// This version stores all codebook bytes into a single contiguous pool.
// DecoderMap entries point into that pool; DecoderMap does not own/free sequences.

// decompress_header.c
//
// Header layout description unchanged (see previous comments).
// This version stores all codebook bytes into a single contiguous pool.
// DecoderMap entries point into that pool; DecoderMap does not own/free sequences.

#include "decompress_header.h"
#include "bit_reader.h"
#include "code_classes.h"
#include "decoder_map.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Global decoder map and the class2 bit-width for this file */
DecoderMap decoder_map;
uint8_t global_class2_bits = 0;
uint8_t global_rle_bits = 0;

/* ---- Single contiguous pool for all codebook sequence bytes ---- */

static uint8_t *g_codebook_pool = NULL;
static size_t g_codebook_pool_cap = 0;
static size_t g_codebook_pool_used = 0;


static void codebook_pool_init(size_t cap) {
    // Free any existing pool first to avoid memory leaks
    if (g_codebook_pool != NULL) {
        free(g_codebook_pool);
        g_codebook_pool = NULL;
    }
    
    if (cap == 0) {
        g_codebook_pool = NULL;
        g_codebook_pool_cap = 0;
        g_codebook_pool_used = 0;
        return;
    }
    
    g_codebook_pool = (uint8_t *)malloc(cap);
    if (!g_codebook_pool) {
        fprintf(stderr, "OOM allocating codebook pool of %zu bytes\n", cap);
        exit(EXIT_FAILURE);
    }
    g_codebook_pool_cap = cap;
    g_codebook_pool_used = 0;
}

static uint8_t *codebook_pool_alloc(size_t n) {
    if (g_codebook_pool_used + n > g_codebook_pool_cap) {
        fprintf(stderr, "Header error: codebook_pool overflow (need %zu, have %zu)\n", g_codebook_pool_used + n,
                g_codebook_pool_cap);
        exit(EXIT_FAILURE);
    }
    uint8_t *p = g_codebook_pool + g_codebook_pool_used;
    g_codebook_pool_used += n;
    return p;
}

void free_decoder_codebook_pool(void) {
    free(g_codebook_pool);
    g_codebook_pool = NULL;
    g_codebook_pool_cap = 0;
    g_codebook_pool_used = 0;
}

/* ---- Bitreader helper ---- */

#define SAFE_BITREAD(reader_ptr, out_var, bits)                                                                        \
    do {                                                                                                               \
        if (!bitreader_read((reader_ptr), (out_var), (bits))) {                                                        \
            if (!bitreader_fill_next_chunk((reader_ptr))) {                                                            \
                fprintf(stderr, "Failed to read %u bits (EOF/overflow)\n", (unsigned)(bits));                          \
                exit(EXIT_FAILURE);                                                                                    \
            }                                                                                                          \
            if (!bitreader_read((reader_ptr), (out_var), (bits))) {                                                    \
                fprintf(stderr, "bitreader_read failed after refill for %u bits\n", (unsigned)(bits));                 \
                exit(EXIT_FAILURE);                                                                                    \
            }                                                                                                          \
        }                                                                                                              \
    } while (0)

/* Derive per-class counts from total N */
static void derive_class_counts(uint16_t N, uint16_t cap0, uint16_t cap1, uint16_t cap2, uint16_t out_counts[3]) {
    uint16_t n0 = (N < cap0) ? N : cap0;
    uint16_t rem = (N > n0) ? (N - n0) : 0;
    uint16_t n1 = (rem < cap1) ? rem : cap1;
    uint16_t n2 = (N > (n0 + n1)) ? (N - n0 - n1) : 0;
    out_counts[0] = n0;
    out_counts[1] = n1;
    out_counts[2] = n2;
}

/* Compute a safe upper bound for total bytes needed by all sequences. */
static size_t compute_pool_upper_bound(const uint16_t counts[3], const uint8_t len_bits[3]) {
    size_t total = 0;
    for (int cls = 0; cls < 3; ++cls) {
        uint32_t lb = len_bits[cls];
        /* length = 1 if lb==0, else length <= 2^lb (since length-1 is stored on lb bits) */
        uint32_t per_entry_max = (lb == 0) ? 1u : (1u << lb);
        /* Our DecoderMapEntry.length is uint8_t; keep it bounded to 255. */
        if (per_entry_max > 255u) per_entry_max = 255u;
        total += (size_t)counts[cls] * (size_t)per_entry_max;
    }
    /* Guard against overflow to be safe */
    return total;
}

/* Main header reader */
void read_header_and_create_decoder_map(BitReader *reader) {
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

    // In read_header_and_create_decoder_map function:
    if (num_codes == 0) {
        global_class2_bits = 0;
        global_rle_bits = 0;
        // Free any existing decoder map to avoid leaks
        free_decoder_map(&decoder_map);
        init_decoder_map(&decoder_map, 0); // Reinitialize with capacity 0
        bitreader_move_byte_boundary(reader);
        return;
    }

    /* Read class2_bits (one byte) */
    uint32_t cb = 0;
    SAFE_BITREAD(reader, &cb, 8);
    global_class2_bits = (uint8_t)cb;

    /* Read per-class len_bits (one byte each) */
    uint32_t lb0 = 0, lb1 = 0, lb2 = 0;
    SAFE_BITREAD(reader, &lb0, 8);
    SAFE_BITREAD(reader, &lb1, 8);
    SAFE_BITREAD(reader, &lb2, 8);

    // Read RLE bits (3 bits)
    uint32_t rle_bits_val = 0;
    SAFE_BITREAD(reader, &rle_bits_val, 3);
    global_rle_bits = (uint8_t)rle_bits_val;

    uint8_t len_bits[3];
    len_bits[0] = (uint8_t)lb0;
    len_bits[1] = (uint8_t)lb1;
    len_bits[2] = (uint8_t)lb2;

#ifdef DEBUG
    printf("[DEBUG] class2_bits=%u len_bits=[%u,%u,%u] rle_bits=%u\n", (unsigned)global_class2_bits,
           (unsigned)len_bits[0], (unsigned)len_bits[1], (unsigned)len_bits[2], (unsigned)global_rle_bits);
#endif

    /* Derive capacities and counts */
    uint16_t cap0 = get_code_class_threshold(0, 0);
    uint16_t cap1 = get_code_class_threshold(1, 0);
    uint16_t cap2 = get_code_class_threshold(2, global_class2_bits);

    uint16_t counts[3];
    derive_class_counts(num_codes, cap0, cap1, cap2, counts);

    if (counts[2] > cap2) {
        fprintf(stderr, "Header error: class2 needs %u entries but capacity is %u (class2_bits=%u)\n",
                (unsigned)counts[2], (unsigned)cap2, (unsigned)global_class2_bits);
        exit(EXIT_FAILURE);
    }

    /* Allocate a single contiguous pool large enough for the worst case */
    size_t pool_cap = compute_pool_upper_bound(counts, len_bits);
    codebook_pool_init(pool_cap);

#ifdef DEBUG
    printf("[DEBUG] Derived class counts: n0=%u, n1=%u, n2=%u (caps: %u,%u,%u), pool_cap=%zu\n", (unsigned)counts[0],
           (unsigned)counts[1], (unsigned)counts[2], (unsigned)cap0, (unsigned)cap1, (unsigned)cap2, pool_cap);
#endif

    /* Read entries in class order (0,1,2) */
    for (int cls = 0; cls <= 2; ++cls) {
        uint16_t n_here = counts[cls];
        uint8_t lb = len_bits[cls];

        for (uint16_t idx = 0; idx < n_here; ++idx) {
            uint32_t length = 0;

            if (lb > 0) {
                uint32_t lminus = 0;
                SAFE_BITREAD(reader, &lminus, lb);
                length = lminus + 1u;
            } else {
                length = 1u;
            }

            if (length == 0 || length > 255u) {
                fprintf(stderr, "Header error: decoded invalid length=%u for class %d index %u\n", (unsigned)length,
                        cls, (unsigned)idx);
                exit(EXIT_FAILURE);
            }

            /* Allocate slice from the pool; map will point into this block (no per-entry free) */
            uint8_t *seq_dst = codebook_pool_alloc((size_t)length);

            /* Read sequence bytes */
            for (uint32_t b = 0; b < length; ++b) {
                uint32_t bv = 0;
                SAFE_BITREAD(reader, &bv, 8);
                seq_dst[b] = (uint8_t)bv;
            }

            /* Store mapping: index-within-class = idx, class = cls */
            if (!decoder_map_set(&decoder_map, (uint16_t)idx, (uint8_t)cls, (const uint8_t *)seq_dst,
                                 (uint8_t)length)) {
                fprintf(stderr, "decoder_map_set failed for class=%u idx=%u\n", (unsigned)cls, (unsigned)idx);
                exit(EXIT_FAILURE);
            }
        }
    }

#ifdef DEBUG
    printf("[DEBUG] Completed DecoderMap reconstruction (num_codes=%u, pool_used=%zu)\n", (unsigned)num_codes,
           g_codebook_pool_used);
    print_decoder_map(&decoder_map);
#endif

    /* Align to next byte boundary so the body reader starts at a byte boundary (if needed) */
    bitreader_move_byte_boundary(reader);
}
