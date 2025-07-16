#include "compressed_body.h"
#include "compressed_header.h"
#include "bit_writer.h"
#include "../map/code_map.h"
#include "code_classes.h"
#include "../graph/graph.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define BODY_BUFFER_SIZE 4096
extern CodeMap code_map;

#define SAFE_BITWRITE(bw, value, bits)                          \
    do {                                                        \
        if (!bitwriter_write(bw, value, bits)) {                \
            bitwriter_flush(bw);                                \
            bitwriter_write_to_file(bw, file_to_write);         \
            bitwriter_reset(bw);                                \
            if (!bitwriter_write(bw, value, bits)) {            \
                fprintf(stderr, "bitwriter_write failed after flush\n"); \
                free(buffer);                                   \
                exit(EXIT_FAILURE);                             \
            }                                                   \
        }                                                       \
    } while (0)

void populate_body(BestPathView best_path, const uint8_t* block, FILE* file_to_write) {
    uint8_t* buffer = malloc(BODY_BUFFER_SIZE);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate body buffer\n");
        return;
    }

    BitWriter writer;
    bitwriter_init(&writer, buffer, BODY_BUFFER_SIZE);

    for (int32_t i = best_path.path_size - 1; i >= 0; --i) {
        GraphNode* node = get_graph_node(best_path.nodes[i]);
        if (!node || node->node_id == 0) continue; // skip root

        const uint8_t* seq = &block[node->offset];
        uint8_t len = node->sequence_length;
        uint32_t freq = best_path.freqs[i];

        if (len == 1 || freq == 1) {
            // Uncompressed case
            for (uint8_t j = 0; j < len; ++j) {
                SAFE_BITWRITE(&writer, 0, 1); // prefix bit 0
                SAFE_BITWRITE(&writer, seq[j], 8);
            }
        }
        else if (node->is_RLE) {
            // RLE case
            if (node->repeat_seq_length > 8) {
                fprintf(stderr, "RLE repeat_seq_length too large: %u\n", node->repeat_seq_length);
                free(buffer);
                exit(EXIT_FAILURE);
            }            

            SAFE_BITWRITE(&writer, 1, 1); // prefix bit 1
            SAFE_BITWRITE(&writer, node->repeat_seq_length, 3);
            SAFE_BITWRITE(&writer, node->length_of_RLE, 8);

            for (uint8_t j = 0; j < node->repeat_seq_length; ++j) {
                SAFE_BITWRITE(&writer, seq[j], 8);
            }
        }
        else {
            // Compressed non-RLE
            uint16_t code;
            uint8_t code_class;
            if (!code_map_get(&code_map, seq, len, &code, &code_class)) {
                fprintf(stderr, "Missing code_map entry for compressed sequence (offset=%u, len=%u)\n", node->offset, len);
                free(buffer);
                exit(EXIT_FAILURE);
            }
            SAFE_BITWRITE(&writer, 1, 1); // prefix bit 1
            SAFE_BITWRITE(&writer, code_class, 2);
            SAFE_BITWRITE(&writer, code, get_code_class_size(code_class));
        }
    }

    bitwriter_flush(&writer);
    bitwriter_write_to_file(&writer, file_to_write);
    free(buffer);
}
