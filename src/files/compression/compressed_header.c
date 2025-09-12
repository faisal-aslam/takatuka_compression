// compressed_header.c
//
// Updated header writer to emit the compact format:
//  - 16 bits : total number of codewords (placeholder then overwritten)
//  - 8 bits  : class2_bits
//  - 8 bits  : len_bits[0]
//  - 8 bits  : len_bits[1]
//  - 8 bits  : len_bits[2]
// Followed by, for class = 0..2 and for each entry in class index order:
//  - if len_bits[class] > 0: write (length - 1) in len_bits[class] bits
//  - write (length) bytes: the raw sequence
//
// This file keeps your original candidate selection, sorting and greedy class
// assignment behavior; only the header emission is changed.

#include "compressed_header.h"
#include "bit_writer.h"
#include "code_classes.h"
#include "code_map.h"
#include "compressed_body.h"
#include "graph.h"
#include "seq_freq_map.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEADER_BUFFER_SIZE 4096

/* exported globals declared in compressed_header.h */
uint8_t global_rle_bits;
uint8_t global_class2_bits = 0;
CodeMap code_map;

typedef struct {
    uint64_t savings;
    const uint8_t *sequence; // pointer into block (no copy)
    uint8_t length;
} CodeCandidate;

/* Compare descending by savings so highest savings are encoded first */
static int compare_candidates_desc(const void *a, const void *b) {
    const CodeCandidate *ca = (const CodeCandidate *)a;
    const CodeCandidate *cb = (const CodeCandidate *)b;
    return (cb->savings > ca->savings) - (cb->savings < ca->savings);
}

/* Helper: whether a node should be skipped for header code creation */
static inline bool should_skip_node(const GraphNode *node, uint32_t freq) {
    return (node->sequence_length == 1) || (freq == 1) || (node->RLE_type) || (node->node_id == 0);
}

/* Check whether the sequence at block[offset] has already been seen; if not, mark it with freq2 */
static inline uint8_t sequence_seen(const uint8_t *block, uint8_t length, uint32_t freq2) {
    uint32_t freq, node_id;
    seq_freq_get(block, length, &freq, &node_id);
    if (freq >= 1) return 1;
    seq_freq_set(block, length, freq2, 0); // set it for future use.
    return 0;
}

/* Helper: compute ceil_log2 for positive integer x (returns 0 for x<=1) */
static uint8_t ceil_log2_u32(uint32_t x) {
    if (x <= 1) return 0;
    uint8_t n = 0;
    uint32_t v = 1u;
    while (v < x) {
        v <<= 1;
        n++;
    }
    return n;
}

static inline uint8_t calculate_bit_length(uint16_t value) {
    if (value == 0) return 1; // At least 1 bit needed to represent 0
    uint8_t bits = 0;
    while (value > 0) {
        bits++;
        value >>= 1;
    }
    return bits;
}

long populate_header(BestPathView best_path, const uint8_t *block, FILE *file_to_write, BitWriter *writer) {
    init_seq_freq_map();

#ifdef DEBUG
    printf("[DEBUG] Initialized sequence frequency map with capacity: %u\n", best_path.path_size * 2);
    printf("[DEBUG] Initial BitWriter state:\n");
    bitwriter_print_state(writer);
#endif

    // Reserve first two bytes (16 bits) for number of codes (we will overwrite later).
    SAFE_BITWRITE(writer, 0, 16, file_to_write, "header_length"); // placeholder
    size_t header_start_bit = writer->byte_pos * 8 + writer->bit_pos - 16;

#ifdef DEBUG
    printf("[DEBUG] Reserved 16 bits for code count at bit position %zu\n", header_start_bit);
    bitwriter_print_state(writer);
#endif

    // Collect candidates
    CodeCandidate *candidates = malloc(sizeof(CodeCandidate) * best_path.path_size);
    if (!candidates) {
        fprintf(stderr, "Failed to allocate candidates array\n");
        exit(EXIT_FAILURE);
    }
    int candidate_count = 0;
    uint16_t max_rle_count = 0;
    for (int32_t i = best_path.path_size - 1; i >= 0; --i) {
        GraphNode *node = get_graph_node(best_path.nodes[i]);
        if (!node) continue;
        // Track maximum RLE count
        if (node->RLE_type && node->length_of_RLE > max_rle_count) {
            max_rle_count = node->length_of_RLE;
        }
        uint32_t freq = best_path.freqs[i];
        uint8_t len = node->sequence_length;
        if (should_skip_node(node, freq)) continue;
        uint32_t offset = node->offset;
        if (sequence_seen(&block[offset], len, freq)) continue;
        const uint8_t *sequence = &block[offset];
        candidates[candidate_count++] = (CodeCandidate){.savings = (uint64_t)freq, .sequence = sequence, .length = len};
    }
    // Calculate RLE bits using the helper function
    if (max_rle_count > 0) {
        global_rle_bits = calculate_bit_length(max_rle_count);
        // Cap at 8 bits since RLE count is uint8_t
        if (global_rle_bits > 8) global_rle_bits = 8;
    } else {
        global_rle_bits = 0; // No RLE sequences
    }

    printf("Maximum RLE count: %u, using %u bits for RLE encoding\n", max_rle_count, global_rle_bits);


    // Sort and init code_map
    qsort(candidates, candidate_count, sizeof(CodeCandidate), compare_candidates_desc);
    init_code_map(&code_map, candidate_count);

#ifdef DEBUG
    printf("[DEBUG] Sorted %d candidates, initialized code map\n", candidate_count);
#endif

    // Fixed capacities class0 and class1
    uint32_t max_class0 = get_code_class_threshold(0, 0);
    uint32_t max_class1 = get_code_class_threshold(1, 0);

    // Plan greedy fill counts (without writing yet)
    uint32_t will_fill_class0 = (candidate_count <= (int)max_class0) ? (uint32_t)candidate_count : max_class0;
    uint32_t remaining_after_class0 = (uint32_t)candidate_count - will_fill_class0;
    uint32_t will_fill_class1 = (remaining_after_class0 <= max_class1) ? remaining_after_class0 : max_class1;
    uint32_t will_fill_class2 = (uint32_t)candidate_count - will_fill_class0 - will_fill_class1;

#ifdef DEBUG
    printf("[DEBUG] planned fill: class0=%u, class1=%u, class2=%u\n", will_fill_class0, will_fill_class1,
           will_fill_class2);
#endif

    // Determine class2 bits and capacities
    uint8_t class2_bits = calculate_class2_bits((uint16_t)will_fill_class2);
    global_class2_bits = class2_bits;

    printf("Total number of codes=%d, Computed class2_bits = %u\n", candidate_count, class2_bits);

    uint32_t max_class2 = get_code_class_threshold(2, class2_bits);

    printf("Capacities after class2_bits: class0=%u, class1=%u, class2=%u\n", max_class0, max_class1, max_class2);

    // Simulate assignment to determine per-class ordering and index-within-class
    uint8_t *assigned_class = malloc((size_t)candidate_count);
    uint16_t *index_within_class = malloc(sizeof(uint16_t) * (size_t)candidate_count);
    uint16_t assigned_count[3] = {0, 0, 0};
    uint32_t max_per_class[3] = {max_class0, max_class1, max_class2};

    if (!assigned_class || !index_within_class) {
        fprintf(stderr, "OOM simulating assignment\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < candidate_count; ++i) {
        int cls = -1;
        for (int c = 0; c <= 2; ++c) {
            if (assigned_count[c] < max_per_class[c]) {
                cls = c;
                break;
            }
        }
        if (cls == -1) {
            fprintf(stderr, "Error: All code classes full during simulation at i=%d\n", i);
            free(candidates);
            exit(EXIT_FAILURE);
        }
        assigned_class[i] = (uint8_t)cls;
        index_within_class[i] = assigned_count[cls];
        assigned_count[cls]++;
    }

    // Compute maximum length per class
    uint32_t max_len[3] = {0, 0, 0};
    for (int i = 0; i < candidate_count; ++i) {
        int cls = assigned_class[i];
        if (candidates[i].length > max_len[cls]) max_len[cls] = candidates[i].length;
    }

    // Determine len_bits per class:
    // len_bits[c] = minimum bits to represent (length_minus_one) in [0 .. max_len[c]-1]
    // If max_len[c] <= 1, len_bits[c] == 0 (no bits stored; decoder must treat length=1)
    uint8_t len_bits[3];
    for (int c = 0; c < 3; ++c) {
        len_bits[c] = ceil_log2_u32((uint32_t)(max_len[c] > 0 ? max_len[c] : 1));
    }

#ifdef DEBUG
    printf("[DEBUG] len_bits per class: [%u, %u, %u]\n", len_bits[0], len_bits[1], len_bits[2]);
#endif

    // Write class2_bits and len_bits (each as one byte)
    SAFE_BITWRITE(writer, class2_bits, 8, file_to_write, "class2_bits");
    SAFE_BITWRITE(writer, len_bits[0], 8, file_to_write, "len_bits0");
    SAFE_BITWRITE(writer, len_bits[1], 8, file_to_write, "len_bits1");
    SAFE_BITWRITE(writer, len_bits[2], 8, file_to_write, "len_bits2");

    // === ADD THIS RIGHT HERE ===
    SAFE_BITWRITE(writer, global_rle_bits, 3, file_to_write, "rle_bits");
    // ===========================

#ifdef DEBUG
    printf("[DEBUG] Wrote class2_bits and len_bits header fields\n");
    bitwriter_print_state(writer);
#endif

    // Build per-class index -> candidate index mapping so we can write in class order
    uint16_t *per_class_offsets[3] = {NULL, NULL, NULL};
    for (int c = 0; c < 3; ++c) {
        if (assigned_count[c] > 0) {
            per_class_offsets[c] = malloc(sizeof(uint16_t) * assigned_count[c]);
            if (!per_class_offsets[c]) {
                fprintf(stderr, "OOM allocating per_class_offsets\n");
                exit(EXIT_FAILURE);
            }
            for (uint16_t k = 0; k < assigned_count[c]; ++k)
                per_class_offsets[c][k] = UINT16_MAX;
        }
    }

    for (int i = 0; i < candidate_count; ++i) {
        int cls = assigned_class[i];
        uint16_t idx = index_within_class[i];
        per_class_offsets[cls][idx] = (uint16_t)i;
    }

    // Emit entries in class order (and populate code_map with index-within-class)
    for (int cls = 0; cls <= 2; ++cls) {
        uint16_t cnt = assigned_count[cls];
        uint8_t lb = len_bits[cls];
        for (uint16_t idx = 0; idx < cnt; ++idx) {
            uint16_t cand_i = per_class_offsets[cls][idx];
            CodeCandidate *cand = &candidates[cand_i];

            // Register into code_map: index within class = idx
            code_map_set(&code_map, cand->sequence, cand->length, idx, (uint8_t)cls);

            uint32_t length = cand->length;
            if (lb > 0) {
                uint32_t value = length - 1; // encode length_minus_one
                SAFE_BITWRITE(writer, value, lb, file_to_write, "seq_len_compact");
            }
            // Write sequence bytes
            for (uint32_t j = 0; j < length; ++j) {
                SAFE_BITWRITE(writer, cand->sequence[j], 8, file_to_write, "seq_byte");
            }
        }
    }

#ifdef DEBUG
    printf("[DEBUG] Final code map state:\n");
    print_code_map(&code_map);
    printf("[DEBUG] Header writing complete, flushing...\n");
#endif

    // Overwrite placeholder with actual count
    bitwriter_overwrite_at(writer, header_start_bit, candidate_count, 16);

    // Flush to file
    long header_start = ftell(file_to_write);
    if (!bitwriter_write_to_file(writer, file_to_write)) {
        fprintf(stderr, "Failed to write header to file\n");
        exit(EXIT_FAILURE);
    }
    long header_end = ftell(file_to_write);

    printf("Header size written: %ld bytes\n", header_end - header_start);

    // Reset writer positions and cleanup
    bitwriter_reset_positions(writer);

    free(candidates);
    free(assigned_class);
    free(index_within_class);
    for (int c = 0; c < 3; ++c) {
        if (per_class_offsets[c]) free(per_class_offsets[c]);
    }

    return header_end - header_start;
}
