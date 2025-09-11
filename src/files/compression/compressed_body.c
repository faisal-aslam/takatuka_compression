// compressed_body.c
//
// Writes the compressed body using the code_map filled by populate_header().
// Uses global_class2_bits (written in header) to determine class-2 code widths.

#include "compressed_body.h"
#include "bit_writer.h"
#include "code_classes.h"
#include "code_map.h"
#include "compressed_header.h" // for global_class2_bits and global_rle_bits
#include "graph.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

extern CodeMap code_map; // filled by populate_header()
extern uint8_t global_class2_bits; // from compressed_header.h  
extern uint8_t global_rle_bits;    // from compressed_header.h

// === FILE: compressed_body.c ===

long populate_body(BestPathView best_path, const uint8_t *block, FILE *file_to_write, BitWriter *writer) {
#ifdef DEBUG
    printf("\n\n\n******************************* \n\n[DEBUG] Starting body population with path size: %d\n",
           best_path.path_size);
    printf("[DEBUG] Initial BitWriter state:\n\n\n");
    bitwriter_print_state(writer);
#endif

    for (int32_t i = best_path.path_size - 1; i >= 0; --i) {
        GraphNode *node = get_graph_node(best_path.nodes[i]);
        if (!node || node->node_id == 0) {
            continue; // skip root or null
        }

        const uint8_t *seq = &block[node->offset];
        uint8_t len = node->sequence_length;

#ifdef DEBUG
        printf("[DEBUG] Processing node %d: offset=%u, len=%u, RLE_type=%d\n",
               node->node_id, node->offset, len, node->RLE_type);
#endif

        uint16_t code;
        uint8_t code_class;

        if (node->RLE_type == 1) {
            // ===== Uniform RLE =====
            // Layout:
            //   flag (1b=1) | class=11b (2b) | length_of_RLE (global_rle_bits) | symbol (8b)
#ifdef DEBUG
            printf("[DEBUG] Writing Uniform RLE (count=%u, global_rle_bits=%u)\n",
                   node->length_of_RLE, global_rle_bits);
#endif
            SAFE_BITWRITE(writer, 1, 1, file_to_write, "uniformRLE_flag");
            SAFE_BITWRITE(writer, 3, 2, file_to_write, "code_class");

            uint8_t rle_bits = (global_rle_bits > 0) ? global_rle_bits : 8;
            SAFE_BITWRITE(writer, node->length_of_RLE, rle_bits, file_to_write, "len_of_RLE");
            SAFE_BITWRITE(writer, seq[0], 8, file_to_write, "RLE_symbol");

        } else if (node->RLE_type == 2) {
            // ===== Arithmetic RLE =====
            // Layout:
            //   flag (1b=0) | class=11b (2b) | start (8b) | end (8b)
#ifdef DEBUG
            printf("[DEBUG] Writing Arithmetic RLE: start=%02X end=%02X\n",
                   node->RLE_start, node->RLE_end);
#endif
            SAFE_BITWRITE(writer, 0, 1, file_to_write, "arithRLE_flag");
            SAFE_BITWRITE(writer, 3, 2, file_to_write, "code_class");

            SAFE_BITWRITE(writer, node->RLE_start, 8, file_to_write, "arithRLE_start");
            SAFE_BITWRITE(writer, node->RLE_end, 8, file_to_write, "arithRLE_end");

        } else if (len > 1 && code_map_get(&code_map, seq, len, &code, &code_class)) {
            // ===== Normal compressed sequence =====
            // Layout:
            //   flag (1b=1) | class (2b) | code (variable bits)
            SAFE_BITWRITE(writer, 1, 1, file_to_write, "compressed_flag");
            SAFE_BITWRITE(writer, code_class, 2, file_to_write, "code_class");

            uint8_t bits_for_code = get_code_class_size(code_class, global_class2_bits);
            SAFE_BITWRITE(writer, code, bits_for_code, file_to_write, "code");

        } else {
            // ===== Raw bytes =====
            // Layout per byte:
            //   flag (1b=0) | symbol (8b)
            for (uint8_t j = 0; j < len; ++j) {
                SAFE_BITWRITE(writer, 0, 1, file_to_write, "uncompressed_flag");
                SAFE_BITWRITE(writer, seq[j], 8, file_to_write, "raw_byte");
            }
        }
    }

    long body_start = ftell(file_to_write);
    if (!bitwriter_write_to_file(writer, file_to_write)) {
        fprintf(stderr, "Failed to write final body data to file\n");
        exit(EXIT_FAILURE);
    }
    long body_end = ftell(file_to_write);

    printf("Body size written: %ld bytes\n", body_end - body_start);
    return body_end - body_start;
}
