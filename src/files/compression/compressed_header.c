//compressed_header.c

#include "compressed_header.h"
#include "bit_writer.h"
#include "code_classes.h"
#include "../graph/graph.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "seq_freq_map.h"
#include "code_map.h"
#include "compressed_body.h"

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

void populate_header(BestPathView best_path, const uint8_t* block, 
                    FILE* file_to_write, BitWriter* writer) {
    init_seq_freq_map(&seq_map, best_path.path_size * 2);

#ifdef DEBUG
    printf("[DEBUG] Initialized sequence frequency map with capacity: %u\n", best_path.path_size * 2);
    printf("[DEBUG] Initial BitWriter state:\n");
    bitwriter_print_state(writer);
#endif

    // Reserve first two bytes for number of codes
    SAFE_BITWRITE(writer, 0, 16, file_to_write); // placeholder
    size_t header_start_bit = writer->byte_pos * 8 + writer->bit_pos - 16;

#ifdef DEBUG
    printf("[DEBUG] Reserved 16 bits for code count at bit position %zu\n", header_start_bit);
    bitwriter_print_state(writer);
#endif

    uint16_t assigned[3] = {0};
    uint32_t max_per_class[3] = {
        get_code_class_threshold(0),
        get_code_class_threshold(1),
        get_code_class_threshold(2)
    };

#ifdef DEBUG
    printf("[DEBUG] Code class thresholds: Class0=%u, Class1=%u, Class2=%u\n",
           max_per_class[0], max_per_class[1], max_per_class[2]);
#endif

    // Step 1: Collect unique sequences with savings
    CodeCandidate* candidates = malloc(sizeof(CodeCandidate) * best_path.path_size);
    if (!candidates) {
        fprintf(stderr, "Failed to allocate candidates array\n");
        exit(EXIT_FAILURE);
    }
    int candidate_count = 0;

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
            printf("[DEBUG] Skipping node %d (len=%u, freq=%u, RLE=%d)\n", 
                   node->node_id, len, freq, node->is_RLE);
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
        candidates[candidate_count++] = (CodeCandidate){
            .savings = (uint64_t)len * freq,
            .sequence = sequence,
            .length = len
        };

#ifdef DEBUG
        printf("[DEBUG] Added candidate #%d: len=%u, savings=%lu, offset=%u\n", 
               candidate_count, len, (uint64_t)len * freq, offset);
#endif
    }

    if (candidate_count == 0) {
#ifdef DEBUG
        printf("[DEBUG] No candidates found for header\n");
#endif
        bitwriter_flush(writer);
        if (!bitwriter_write_to_file(writer, file_to_write)) {
            fprintf(stderr, "Failed to write empty header\n");
            exit(EXIT_FAILURE);
        }
        bitwriter_reset(writer);
        free_seq_freq_map(&seq_map);
        free(candidates);
        return;
    }

    // Step 2: Sort descending by savings
    qsort(candidates, candidate_count, sizeof(CodeCandidate), compare_candidates_desc);
    init_code_map(&code_map, candidate_count);

#ifdef DEBUG
    printf("[DEBUG] Sorted %d candidates, initialized code map\n", candidate_count);
    printf("[DEBUG] Candidate savings order:\n");
    for (int i = 0; i < candidate_count && i < 10; i++) {
        printf("  #%d: savings=%lu, len=%u\n", i, candidates[i].savings, candidates[i].length);
    }
    if (candidate_count > 10) {
        printf("  ... and %d more\n", candidate_count - 10);
    }
#endif

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
            fprintf(stderr, "Error: All code classes full after encoding %d sequences\n", i);
            free(candidates);
            exit(EXIT_FAILURE);
        }

        code_map_set(&code_map, cand->sequence, cand->length, assigned[code_class], code_class);

#ifdef DEBUG
        printf("[DEBUG] Encoding candidate #%d (class=%d, code=%u, len=%u): ", 
               i, code_class, assigned[code_class], cand->length);
        for (uint8_t j = 0; j < cand->length && j < 8; ++j) {
            printf("%02X ", cand->sequence[j]);
        }
        if (cand->length > 8) printf("...");
        printf("\n");
#endif

        SAFE_BITWRITE(writer, code_class, 2, file_to_write);
#ifdef DEBUG
        printf("[DEBUG] ➤ Written 2 bits: code_class = %u\n", code_class);
        bitwriter_print_state(writer);
#endif

        SAFE_BITWRITE(writer, cand->length, 8, file_to_write);
#ifdef DEBUG
        printf("[DEBUG] ➤ Written 8 bits: length = %u\n", cand->length);
        bitwriter_print_state(writer);
#endif

        uint8_t code_bits = get_code_class_size(code_class);
        SAFE_BITWRITE(writer, assigned[code_class], code_bits, file_to_write);
#ifdef DEBUG
        printf("[DEBUG] ➤ Written %u bits: code index in class[%u] = %u\n", 
               code_bits, code_class, assigned[code_class]);
        bitwriter_print_state(writer);
#endif

        for (uint8_t j = 0; j < cand->length; ++j) {
            SAFE_BITWRITE(writer, cand->sequence[j], 8, file_to_write);
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

    bitwriter_overwrite_at(writer, header_start_bit, candidate_count, 16);    
    bitwriter_flush(writer);
    
    
    if (!bitwriter_write_to_file(writer, file_to_write)) {
        fprintf(stderr, "Failed to write header to file\n");
        exit(EXIT_FAILURE);
    }

#ifdef DEBUG
    printf("[DEBUG] Header written successfully (%d codes), BitWriter reset\n", candidate_count);
    bitwriter_print_state(writer);
#endif

    bitwriter_reset_positions(writer); // as we have written the buffer in the file. So we can reset it. 
    free_seq_freq_map(&seq_map);
    free(candidates);
}