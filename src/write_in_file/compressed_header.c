#include "compressed_header.h"
#include "bit_writer.h"
#include "code_classes.h"
#include "../graph/graph.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../map/seq_freq_map.h"

#define MAX_CODES 512
#define HEADER_BUFFER_SIZE 4096
static SeqFreqMap map;

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

static inline uint8_t sequence_seen(const uint8_t *block, uint8_t length) {
        if (seq_repo_get_frequency(&map, block, length) >= 1) {
                return 1;
        }
        seq_freq_increment(&map, block, length, -1);
        return 0;
}

void populate_header(BestPathView best_path, const uint8_t* block, FILE* file_to_write) {
    uint8_t* buffer = malloc(HEADER_BUFFER_SIZE);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate header buffer\n");
        return;
    }

    BitWriter writer;
    bitwriter_init(&writer, buffer, HEADER_BUFFER_SIZE);
    init_seq_freq_map(&map, best_path.path_size); 

    uint16_t assigned[3] = {0};

    for (int32_t i = best_path.path_size - 1; i >= 0; --i) {
        GraphNode* node = get_graph_node(best_path.nodes[i]);
        if (!node) continue;
        uint8_t len = node->sequence_length;
        uint32_t freq = best_path.freqs[i];

        if (should_skip_node(node, freq)) continue;
#ifdef DEBUG
        printf("\n[DEBUG] Generating code for node_id=%u (offset=%u, len=%u, freq=%u)\n", 
               node->node_id, node->offset, len, freq);
        print_graph_node(node);
        print_node_sequence(node, block);
#endif


        uint32_t offset = node->offset;
        if (sequence_seen(&block[offset], node->sequence_length)) continue;
        

        uint8_t code_class = select_code_class((uint64_t)len * freq, assigned);
        if (code_class > 2) {
            fprintf(stderr, "Code class exhausted for offset %u, len %u\n", node->offset, len);
            continue;
        }

        bitwriter_write(&writer, code_class, 2);
#ifdef DEBUG
        printf("[DEBUG] ➤ Written 2 bits: code_class = %u\n", code_class);
        bitwriter_print_state(&writer);
#endif

        bitwriter_write(&writer, len, 8);
#ifdef DEBUG
        printf("[DEBUG] ➤ Written 8 bits: length = %u\n", len);
        bitwriter_print_state(&writer);
#endif


        uint8_t code_bits = get_code_class_size(code_class);
        bitwriter_write(&writer, assigned[code_class], code_bits);

#ifdef DEBUG
        printf("[DEBUG] ➤ Written %u bits: code index in class[%u] = %u\n", 
               code_bits, code_class, assigned[code_class]);
        bitwriter_print_state(&writer);
#endif


        for (uint8_t j = 0; j < len; ++j) {
            bitwriter_write(&writer, block[offset + j], 8);
        }
#ifdef DEBUG
        printf("[DEBUG] ➤ Written %u bytes: sequence = ", len);
        for (uint8_t j = 0; j < len; ++j) {
            printf("%02X ", block[offset + j]);
        }
        printf("\n");
        bitwriter_print_state(&writer);
#endif

        assigned[code_class]++;
    }

    bitwriter_flush(&writer);

    // Store header somewhere or return
    // e.g., write to file, or assign to a global CompressedHeaderBytes structure
    bitwriter_write_to_file(&writer, file_to_write);
    free(buffer);
}