#include "compressed_header.h"
#include "../graph/graph.h"
#include "code_classes.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>

#define MAX_CODES 512
bool seen_offsets[BLOCK_SIZE];

static inline uint64_t seqkey_hash(uint32_t offset, uint8_t len) {
    return ((uint64_t)offset << 8) | len;
}

static inline bool should_skip_node(const GraphNode* node, uint32_t freq) {
    return (node->sequence_length == 1) || 
           (freq == 1) || 
           (node->is_RLE) ||
           (node->node_id == 0); // Skip root
}

static inline bool is_duplicate_sequence(uint64_t hash, uint64_t* seen_hashes, uint16_t count) {
    for (uint16_t i = 0; i < count; ++i)
        if (seen_hashes[i] == hash) return true;
    return false;
}

static inline uint8_t select_code_class(uint64_t value, const uint16_t* assigned) {
    uint8_t class = (value < 32) ? 2 : (value < 128) ? 1 : 0;
    while (class <= 2) {
        if (assigned[class] < get_code_class_threshold(class)) return class;
        class++;
    }
    return 3;
}

static Code* create_inline_code(uint8_t code_class, uint8_t seq_len, const uint8_t* block_data, uint32_t offset) {
    uint8_t code_bits = get_code_class_size(code_class);
    uint8_t code_bytes = (code_bits + 7) / 8;
    uint8_t total_bytes = code_bytes + seq_len;

    size_t total_size = sizeof(Code) + total_bytes;
    Code* code = malloc(total_size);
    if (!code) return NULL;

    code->code_class = code_class;
    code->seq_length = seq_len;

    memset(code->data, 0, code_bytes);  // Placeholder for actual code bits
    memcpy(code->data + code_bytes, &block_data[offset], seq_len);

    return code;
}

void populate_header(BestPathView best_path, const uint8_t* block) {
    static Code* code_ptrs[MAX_CODES];
    uint16_t assigned[3] = {0};
    uint16_t code_index = 0;
    uint16_t seen_count = 0;
    

    for (int32_t i = best_path.path_size - 1; i >= 0; --i) {
        GraphNode* node = get_graph_node(best_path.nodes[i]);
        if (!node) continue;

        uint8_t len = node->sequence_length;
        uint32_t freq = best_path.freqs[i];

        if (should_skip_node(node, freq)) continue;

        uint32_t offset = node->offset;
        if (seen_offsets[offset]) continue;
        seen_offsets[offset] = true;

        uint8_t code_class = select_code_class((uint64_t)len * freq, assigned);
        if (code_class > 2) {
            fprintf(stderr, "Code class exhausted for offset %u, len %u\n", node->offset, len);
            continue;
        }

        Code* code = create_inline_code(code_class, len, block, node->offset);
        if (!code) {
            fprintf(stderr, "Allocation failed for code entry\n");
            continue;
        }

        code_ptrs[code_index++] = code;
        assigned[code_class]++;
    }

    // Finalize header
    CompressedHeader* header = malloc(sizeof(CompressedHeader));
    header->number_of_codes = code_index;
    header->codes = malloc(sizeof(Code*) * code_index);
    for (uint16_t i = 0; i < code_index; ++i) {
        header->codes[i] = code_ptrs[i];
    }

    // Optionally assign globally or return/store elsewhere
    // Example:
    // global_header = header;

    // Or pass header pointer out from here
}


void free_compressed_header(CompressedHeader* header) {
    if (!header) return;
    for (uint16_t i = 0; i < header->number_of_codes; ++i) {
        free(header->codes[i]);
    }
    free(header->codes);
    free(header);
}
