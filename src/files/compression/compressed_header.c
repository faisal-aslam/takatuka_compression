// compressed_header.c

// compressed_header.c
//
// Writes the compressed header and builds the code_map used by encoder/decoder.
// This version supports a dynamic class2 bit-width (written as one byte into header).
// Class 0 and Class 1 remain fixed-size (get_code_class_size(...)), class 3 is RLE.

#include "compressed_header.h"
#include "bit_writer.h"
#include "code_classes.h"
#include "code_map.h"
#include "compressed_body.h" // for put prototypes if needed
#include "graph.h"
#include "seq_freq_map.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEADER_BUFFER_SIZE 4096

// Global exported variable: number of bits used for class 2 indices for this file.
// This is set by populate_header() and consumed by populate_body().
uint8_t global_class2_bits = 0;
CodeMap code_map;
typedef struct {
    uint64_t savings;
    const uint8_t *sequence; // pointer into block (no copy)
    uint8_t length;
} CodeCandidate;

/* Compare descending by savings so highest savings are encoded first */
int compare_candidates_desc(const void *a, const void *b) {
    const CodeCandidate *ca = (const CodeCandidate *)a;
    const CodeCandidate *cb = (const CodeCandidate *)b;
    return (cb->savings > ca->savings) - (cb->savings < ca->savings);
}

/* Helper: whether a node should be skipped for header code creation */
static inline bool should_skip_node(const GraphNode *node, uint32_t freq) {
    return (node->sequence_length == 1) || (freq == 1) || (node->is_RLE) || (node->node_id == 0);
}

/* Check whether the sequence at block[offset] has already been seen; if not, mark it with freq2 */
static inline uint8_t sequence_seen(const uint8_t *block, uint8_t length, uint32_t freq2) {
    uint32_t freq, node_id;
    seq_freq_get(block, length, &freq, &node_id);
    if (freq >= 1) return 1;
    seq_freq_set(block, length, freq2, 0); // set it for future use.
    return 0;
}

void populate_header(BestPathView best_path, const uint8_t *block, FILE *file_to_write, BitWriter *writer) {
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

    // Prepare candidates array (max best_path.path_size)
    CodeCandidate *candidates = malloc(sizeof(CodeCandidate) * best_path.path_size);
    if (!candidates) {
        fprintf(stderr, "Failed to allocate candidates array\n");
        exit(EXIT_FAILURE);
    }
    int candidate_count = 0;

    // Collect unique sequences with their savings
    for (int32_t i = best_path.path_size - 1; i >= 0; --i) {
        GraphNode *node = get_graph_node(best_path.nodes[i]);
        if (!node) {
#ifdef DEBUG
            printf("[DEBUG] Node at position %d is null\n", i);
#endif
            continue;
        }

        uint32_t freq = best_path.freqs[i];
        uint8_t len = node->sequence_length;

        if (should_skip_node(node, freq)) {
#ifdef DEBUG
            printf("[DEBUG] Skipping node %d (len=%u, freq=%u, RLE=%d)\n", node->node_id, len, freq, node->is_RLE);
#endif
            continue;
        }

        uint32_t offset = node->offset;
        if (sequence_seen(&block[offset], len, freq)) {
#ifdef DEBUG
            printf("[DEBUG] Sequence already seen at offset %u (len=%u)\n", offset, len);
#endif
            continue;
        }

        const uint8_t *sequence = &block[offset];
        candidates[candidate_count++] = (CodeCandidate){.savings = (uint64_t)freq, .sequence = sequence, .length = len};

#ifdef DEBUG
        printf("[DEBUG] Added candidate #%d: len=%u, savings=%lu, offset=%u\n", candidate_count, len,
               (uint64_t)len * freq, offset);
#endif
    }

    if (candidate_count == 0) {
#ifdef DEBUG
        printf("[DEBUG] No candidates found for header\n");
#endif

        if (!bitwriter_write_to_file(writer, file_to_write)) {
            fprintf(stderr, "Failed to write empty header\n");
            exit(EXIT_FAILURE);
        }
        bitwriter_reset(writer);
        free(candidates);
        global_class2_bits = 0; // nothing to do in body
        return;
    }

    // Sort candidates descending by savings
    qsort(candidates, candidate_count, sizeof(CodeCandidate), compare_candidates_desc);

    // Initialize code_map for this many candidates
    init_code_map(&code_map, candidate_count);

#ifdef DEBUG
    printf("[DEBUG] Sorted %d candidates, initialized code map\n", candidate_count);
    printf("[DEBUG] Candidate savings order (top 10):\n");
    for (int i = 0; i < candidate_count && i < 10; i++) {
        printf("  #%d: savings=%lu, len=%u\n", i, candidates[i].savings, candidates[i].length);
    }
    if (candidate_count > 10) {
        printf("  ... and %d more\n", candidate_count - 10);
    }
#endif

    // Compute fixed capacities for class 0 and 1 (class2 capacity is dynamic)
    // Use get_code_class_threshold with class2_bits==0 for classes 0 and 1 (unused param)
    uint32_t max_class0 = get_code_class_threshold(0, 0);
    uint32_t max_class1 = get_code_class_threshold(1, 0);

#ifdef DEBUG
    printf("[DEBUG] Fixed capacities: class0=%u, class1=%u\n", max_class0, max_class1);
#endif

    // We will attempt to fill class0 and class1 first (to keep codes small).
    // Determine how many candidates will go into each class without actually assigning codes yet.
    uint32_t will_fill_class0;
    if (candidate_count <= (int)max_class0) {
        will_fill_class0 = (uint32_t)candidate_count;
    } else {
        will_fill_class0 = max_class0;
    }

    uint32_t remaining_after_class0 = (uint32_t)candidate_count - will_fill_class0;

    uint32_t will_fill_class1;
    if (remaining_after_class0 <= max_class1) {
        will_fill_class1 = remaining_after_class0;
    } else {
        will_fill_class1 = max_class1;
    }

    uint32_t will_fill_class2 = (uint32_t)candidate_count - will_fill_class0 - will_fill_class1;

#ifdef DEBUG
    printf("[DEBUG] planned fill: class0=%u, class1=%u, class2=%u\n", will_fill_class0, will_fill_class1,
           will_fill_class2);
#endif

    // Determine minimal bits needed for class2 indices (0 if no class2 entries)
    uint8_t class2_bits = calculate_class2_bits((uint16_t)will_fill_class2);
    global_class2_bits = class2_bits; // export for use by body writer/decoder

    printf("Computed class2_bits = %u\n", class2_bits);

    // Compute actual capacities now (including dynamic class2)
    uint32_t max_class2 = get_code_class_threshold(2, class2_bits);

    printf("Capacities after class2_bits: class0=%u, class1=%u, class2=%u\n", max_class0, max_class1, max_class2);

    // Write the class2_bits byte into the header so decoder knows how many bits to read for class 2.
    // Place it right after the reserved 16-bit code count placeholder.
    SAFE_BITWRITE(writer, class2_bits, 8, file_to_write, "class2_bits");
#ifdef DEBUG
    printf("[DEBUG] ➤ Written 8 bits: class2_bits = %u\n", class2_bits);
    bitwriter_print_state(writer);
#endif

    // Now perform the actual encoding of candidate entries.
    // We'll greedily assign to the smallest class that still has capacity (0, then 1, then 2).
    uint16_t assigned[3] = {0, 0, 0};
    uint32_t max_per_class[3] = {max_class0, max_class1, max_class2};

    for (int i = 0; i < candidate_count; ++i) {
        CodeCandidate *cand = &candidates[i];

        // Find lowest available class (0 -> 1 -> 2)
        int code_class = -1;
        for (int c = 0; c <= 2; ++c) {
            if (assigned[c] < max_per_class[c]) {
                code_class = c;
                break;
            }
        }

        if (code_class == -1) {
            // This should not happen because we sized class2_bits to fit remaining codes,
            // but guard defensively.
            fprintf(stderr, "Error: All code classes full after encoding %d sequences\n", i);
            free(candidates);
            exit(EXIT_FAILURE);
        }

        code_map_set(&code_map, cand->sequence, cand->length, assigned[code_class], (uint8_t)code_class);

    #ifdef DEBUG
        printf("[DEBUG] Encoding candidate #%d (class=%d, code=%u, code_length=3+%u, len=%u): ", i, code_class,
               assigned[code_class], get_code_class_size((uint8_t)code_class, class2_bits), cand->length);
        for (uint8_t j = 0; j < cand->length; ++j) {
            printf("%c", cand->sequence[j]);
        }
        printf("\n");
    #endif

        // Write code_class (2 bits), sequence length (8 bits), then code index (class sized), then sequence bytes
        SAFE_BITWRITE(writer, code_class, 2, file_to_write, "code_class");
#ifdef DEBUG
        printf("[DEBUG] ➤ Written 2 bits: code_class = %u\n", code_class);
        bitwriter_print_state(writer);
#endif

        SAFE_BITWRITE(writer, cand->length, 8, file_to_write, "seq_len");
#ifdef DEBUG
        printf("[DEBUG] ➤ Written 8 bits: length = %u\n", cand->length);
        bitwriter_print_state(writer);
#endif

        uint8_t code_bits = get_code_class_size((uint8_t)code_class, class2_bits);
        SAFE_BITWRITE(writer, assigned[code_class], code_bits, file_to_write, "code_bits");
#ifdef DEBUG
        printf("[DEBUG] ➤ Written %u bits: code index in class[%u] = %u\n", code_bits, code_class,
               assigned[code_class]);
        bitwriter_print_state(writer);
#endif

        for (uint8_t j = 0; j < cand->length; ++j) {
            SAFE_BITWRITE(writer, cand->sequence[j], 8, file_to_write, "");
#ifdef DEBUG
            printf("[DEBUG] ➤ Written byte #%u: %02X\n", j, cand->sequence[j]);
            bitwriter_print_state(writer);
#endif
        }

        assigned[code_class]++;
    }

#ifdef DEBUG
    printf("[DEBUG] Final code map state:\n");
    print_code_map(&code_map);
    printf("[DEBUG] Header writing complete, flushing...\n");
#endif

    // Overwrite the placeholder 16-bit field with the actual candidate_count
    bitwriter_overwrite_at(writer, header_start_bit, candidate_count, 16);

    // Record file offset before flush
    long header_start = ftell(file_to_write);

    // Flush header buffer to file
    if (!bitwriter_write_to_file(writer, file_to_write)) {
        fprintf(stderr, "Failed to write header to file\n");
        exit(EXIT_FAILURE);
    }

    // Record file offset after flush
    long header_end = ftell(file_to_write);

    printf("Header size written: %ld bytes\n", header_end - header_start);
    
#ifdef DEBUG
    printf("[DEBUG] Header written successfully (%d codes), BitWriter reset\n", candidate_count);
    bitwriter_print_state(writer);
#endif

    // Reset writer's internal positions as header buffer has been written to file
    bitwriter_reset_positions(writer);
    free(candidates);
}
