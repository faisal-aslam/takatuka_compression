//compressed_header.c

#include "compressed_header.h"
#include "bit_writer.h"
#include "code_classes.h"
#include "../graph/graph.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../map/seq_freq_map.h"
#include "../map/code_map.h"

#define HEADER_BUFFER_SIZE 4096
static SeqFreqMap seq_map;

CodeMap code_map;
 
static inline bool should_skip_node(const GraphNode* node, uint32_t freq) {
    return (node->sequence_length == 1) || 
           (freq == 1) || 
           (node->is_RLE) || 
           (node->node_id == 0);
}

static inline uint8_t sequence_seen(const uint8_t *block, uint8_t length, uint32_t freq) {
    if (seq_freq_get(&seq_map, block, length, -1) >= 1) return 1;
    seq_freq_set(&seq_map, block, length, freq); //set it for future use.
    return 0;
}

typedef struct {
    uint64_t savings;
    const uint8_t* sequence;  // pointer into block (no copy)
    uint8_t length;
} CodeCandidate;


int compare_candidates_desc(const void* a, const void* b) {
    const CodeCandidate* ca = (const CodeCandidate*)a;
    const CodeCandidate* cb = (const CodeCandidate*)b;
    return (cb->savings > ca->savings) - (cb->savings < ca->savings);
}

void populate_header(BestPathView best_path, const uint8_t* block, FILE* file_to_write) {
    uint8_t* buffer = malloc(HEADER_BUFFER_SIZE);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate header buffer\n");
        return;
    }

    BitWriter writer;
    bitwriter_init(&writer, buffer, HEADER_BUFFER_SIZE);
    init_seq_freq_map(&seq_map, best_path.path_size * 2);

    // Reserve first two bytes for number of codes
    bitwriter_write(&writer, 0, 16); // placeholder
    size_t header_start_bit = 0;

    uint16_t assigned[3] = {0};
    uint32_t max_per_class[3] = {
        get_code_class_threshold(0),
        get_code_class_threshold(1),
        get_code_class_threshold(2)
    };

    // Step 1: collect unique sequences with savings
    CodeCandidate* candidates = malloc(sizeof(CodeCandidate) * best_path.path_size);
    int candidate_count = 0;

    for (int32_t i = best_path.path_size - 1; i >= 0; --i) {
        GraphNode *node = get_graph_node(best_path.nodes[i]);
        if (!node) continue;


        uint32_t freq = best_path.freqs[i];
        uint8_t len = node->sequence_length;

        if (should_skip_node(node, freq)) continue;

        uint32_t offset = node->offset;
        if (sequence_seen(&block[offset], len, freq)) continue;

        // Make a array of the sequences
        const uint8_t *sequence = &block[offset];
        candidates[candidate_count++] = (CodeCandidate){.savings = (uint64_t)len * freq,
                                                        .sequence = sequence,
                                                        .length = len};
    }

    if (candidate_count == 0) { //nothing to be written in the header.
        bitwriter_flush(&writer);
        bitwriter_write_to_file(&writer, file_to_write);
        free_seq_freq_map(&seq_map);
        free(candidates);
        free(buffer);
        return;
    }

    // Step 2: Sort descending by savings
    qsort(candidates, candidate_count, sizeof(CodeCandidate), compare_candidates_desc);
    init_code_map(&code_map, candidate_count);
    // Step 3: Encode in savings order
    for (int i = 0; i < candidate_count; ++i) {
        CodeCandidate* cand = &candidates[i];
        
        // Find lowest available class
        int code_class = -1;
        for (int c = 0; c <= 2; ++c) {
            if (assigned[c] < max_per_class[c]) {
                code_class = c;
                break;
            }
        }

        if (code_class == -1) {
            fprintf(stderr, "Error: All code classes full after encoding %d sequences. Max thresholds: [%u, %u, %u]\n",
                    i, max_per_class[0], max_per_class[1], max_per_class[2]);
            // Clean up
            free(candidates);
            free(buffer);
            exit(EXIT_FAILURE);
        }
        code_map_set(&code_map, cand->sequence, cand->length, assigned[code_class], code_class);

        bitwriter_write(&writer, code_class, 2);
#ifdef DEBUG
        printf("[DEBUG] ➤ Written 2 bits: code_class = %u\n", code_class);
        bitwriter_print_state(&writer);
#endif

        bitwriter_write(&writer, cand->length, 8);
#ifdef DEBUG
        printf("[DEBUG] ➤ Written 8 bits: length = %u\n", cand->length);
        bitwriter_print_state(&writer);
#endif

        bitwriter_write(&writer, assigned[code_class], get_code_class_size(code_class));
#ifdef DEBUG
        printf("[DEBUG] ➤ Written %u bits: code index in class[%u] = %u\n", 
               get_code_class_size(code_class), code_class, assigned[code_class]);
        bitwriter_print_state(&writer);
#endif

        for (uint8_t j = 0; j < cand->length; ++j) {
            bitwriter_write(&writer, cand->sequence[j], 8);
#ifdef DEBUG
        printf("[DEBUG] ➤ Written  byte # %u : sequence = ", j);        
        printf("%02X ", cand->sequence[j]);        
        bitwriter_print_state(&writer);
#endif

        }

        assigned[code_class]++;
    }

    bitwriter_flush(&writer);
    bitwriter_print_state(&writer);
    bitwriter_overwrite_at(&writer, header_start_bit, candidate_count, 16);
    bitwriter_print_state(&writer);
    bitwriter_write_to_file(&writer, file_to_write);    
    bitwriter_print_state(&writer);
    free_seq_freq_map(&seq_map);
    free(candidates);
    free(buffer);
}
