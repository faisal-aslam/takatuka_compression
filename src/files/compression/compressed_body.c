//compressed_body.c

#include "compressed_body.h"
#include "compressed_header.h"
#include "bit_writer.h"
#include "code_map.h"
#include "code_classes.h"
#include "graph.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>


extern CodeMap code_map;

void populate_body(BestPathView best_path, const uint8_t* block, 
                  FILE* file_to_write, BitWriter* writer) {
#ifdef DEBUG
    printf("[DEBUG] Starting body population with path size: %d\n", best_path.path_size);
    printf("[DEBUG] Initial BitWriter state:\n");
    bitwriter_print_state(writer);
#endif

    for (int32_t i = best_path.path_size - 1; i >= 0; --i) {
        GraphNode* node = get_graph_node(best_path.nodes[i]);
        if (!node || node->node_id == 0) {
#ifdef DEBUG
            printf("[DEBUG] Skipping node at position %d (null or root)\n", i);
#endif
            continue;
        }

        const uint8_t* seq = &block[node->offset];
        uint8_t len = node->sequence_length;
        uint32_t freq = best_path.freqs[i];

#ifdef DEBUG
        printf("[DEBUG] Processing node %d: offset=%u, len=%u, freq=%u, RLE=%d\n",
               node->node_id, node->offset, len, freq, node->is_RLE);
#endif

        if (len == 1 || freq == 1) {
            // Uncompressed case
#ifdef DEBUG
            printf("[DEBUG] Writing uncompressed bytes (len=%u)\n", len);
#endif
            for (uint8_t j = 0; j < len; ++j) {
                SAFE_BITWRITE(writer, 0, 1, file_to_write);
#ifdef DEBUG
                printf("[DEBUG] ➤ Written 1 bit: 0 (uncompressed flag)\n");
                bitwriter_print_state(writer);
#endif
                SAFE_BITWRITE(writer, seq[j], 8, file_to_write);
#ifdef DEBUG
                printf("[DEBUG] ➤ Written 8 bits: raw byte %02X\n", seq[j]);
                bitwriter_print_state(writer);
#endif
            }
        }
        else if (node->is_RLE) {
            // RLE case
            if (node->repeat_seq_length > 8) {
                fprintf(stderr, "RLE repeat_seq_length too large: %u\n", node->repeat_seq_length);
                exit(EXIT_FAILURE);
            }

#ifdef DEBUG
            printf("[DEBUG] Writing RLE sequence (repeat_len=%u, count=%u)\n",
                   node->repeat_seq_length, node->length_of_RLE);
#endif

            SAFE_BITWRITE(writer, 1, 1, file_to_write);
#ifdef DEBUG
            printf("[DEBUG] ➤ Written 1 bit: 1 (compressed flag)\n");
            bitwriter_print_state(writer);
#endif
            SAFE_BITWRITE(writer, node->repeat_seq_length, 3, file_to_write);
#ifdef DEBUG
            printf("[DEBUG] ➤ Written 3 bits: RLE repeat length = %u\n", node->repeat_seq_length);
            bitwriter_print_state(writer);
#endif
            SAFE_BITWRITE(writer, node->length_of_RLE, 8, file_to_write);
#ifdef DEBUG
            printf("[DEBUG] ➤ Written 8 bits: RLE count = %u\n", node->length_of_RLE);
            bitwriter_print_state(writer);
#endif

            for (uint8_t j = 0; j < node->repeat_seq_length; ++j) {
                SAFE_BITWRITE(writer, seq[j], 8, file_to_write);
#ifdef DEBUG
                printf("[DEBUG] ➤ Written 8 bits: RLE pattern byte %02X\n", seq[j]);
                bitwriter_print_state(writer);
#endif
            }
        }
        else {
            // Compressed non-RLE
            uint16_t code;
            uint8_t code_class;
            if (!code_map_get(&code_map, seq, len, &code, &code_class)) {
                fprintf(stderr, "Missing code_map entry for compressed sequence\n");
                exit(EXIT_FAILURE);
            }

#ifdef DEBUG
            printf("[DEBUG] Writing compressed sequence (code=%u, class=%u)\n", code, code_class);
#endif

            SAFE_BITWRITE(writer, 1, 1, file_to_write);
#ifdef DEBUG
            printf("[DEBUG] ➤ Written 1 bit: 1 (compressed flag)\n");
            bitwriter_print_state(writer);
#endif
            SAFE_BITWRITE(writer, code_class, 2, file_to_write);
#ifdef DEBUG
            printf("[DEBUG] ➤ Written 2 bits: code class = %u\n", code_class);
            bitwriter_print_state(writer);
#endif
            SAFE_BITWRITE(writer, code, get_code_class_size(code_class), file_to_write);
#ifdef DEBUG
            printf("[DEBUG] ➤ Written %u bits: code = %u\n", 
                   get_code_class_size(code_class), code);
            bitwriter_print_state(writer);
#endif
        }
    }

#ifdef DEBUG
    printf("[DEBUG] Body writing complete, flushing...\n");
#endif

    bitwriter_flush(writer);
    if (!bitwriter_write_to_file(writer, file_to_write)) {
        fprintf(stderr, "Failed to write final body data to file\n");
        exit(EXIT_FAILURE);
    }

#ifdef DEBUG
    printf("[DEBUG] Body written successfully\n");
    bitwriter_print_state(writer);
#endif
}