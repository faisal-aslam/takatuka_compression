#include "compressed_header.h"
#include "bit_writer.h"
#include "code_classes.h"
#include "../graph/graph.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_CODES 512
#define HEADER_BUFFER_SIZE 4096
bool seen_offsets[BLOCK_SIZE];

static inline bool should_skip_node(const GraphNode* node, uint32_t freq) {
    return (node->sequence_length == 1) || 
           (freq == 1) || 
           (node->is_RLE) || 
           (node->node_id == 0);
}

static inline uint8_t select_code_class(uint64_t value, const uint16_t* assigned) {
    uint8_t class = (value < 32) ? 2 : (value < 128) ? 1 : 0;
    while (class <= 2) {
        if (assigned[class] < get_code_class_threshold(class)) return class;
        class++;
    }
    return 3;
}

void populate_header(BestPathView best_path, const uint8_t* block, FILE* file_to_write) {
    uint8_t* buffer = malloc(HEADER_BUFFER_SIZE);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate header buffer\n");
        return;
    }

    BitWriter writer;
    bitwriter_init(&writer, buffer, HEADER_BUFFER_SIZE);

    uint16_t assigned[3] = {0};

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

        bitwriter_write(&writer, code_class, 2);
        bitwriter_write(&writer, len, 8);
        uint8_t code_bits = get_code_class_size(code_class);
        bitwriter_write(&writer, assigned[code_class], code_bits);

        for (uint8_t j = 0; j < len; ++j) {
            bitwriter_write(&writer, block[offset + j], 8);
        }

        assigned[code_class]++;
    }

    bitwriter_flush(&writer);

    // Store header somewhere or return
    // e.g., write to file, or assign to a global CompressedHeaderBytes structure
    bitwriter_write_to_file(&writer, file_to_write);
    free(buffer);
}

void free_compressed_header(CompressedHeader* header) {
    if (!header) return;
    for (uint16_t i = 0; i < header->number_of_codes; ++i) {
        free(header->codes[i]);
    }
    free(header->codes);
    free(header);
}
