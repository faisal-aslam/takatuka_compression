// compressed_body.c

// compressed_body.c
//
// Writes the compressed body using the code_map filled by populate_header().
// Uses global_class2_bits (written in header) to determine class-2 code widths.

#include "compressed_body.h"
#include "bit_writer.h"
#include "code_classes.h"
#include "code_map.h"
#include "compressed_header.h" // for global_class2_bits
#include "graph.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

extern CodeMap code_map; // filled by populate_header()

void populate_body(BestPathView best_path, const uint8_t *block, FILE *file_to_write, BitWriter *writer) {
#ifdef DEBUG
    printf("\n\n\n******************************* \n\n[DEBUG] Starting body population with path size: %d\n",
           best_path.path_size);
    printf("[DEBUG] Initial BitWriter state:\n\n\n");
    bitwriter_print_state(writer);
#endif

    for (int32_t i = best_path.path_size - 1; i >= 0; --i) {
        GraphNode *node = get_graph_node(best_path.nodes[i]);
        if (!node || node->node_id == 0) {
#ifdef DEBUG
            printf("[DEBUG] Skipping node at position %d (null or root)\n", i);
#endif
            continue;
        }

        const uint8_t *seq = &block[node->offset];
        uint8_t len = node->sequence_length;

#ifdef DEBUG
        printf("\n\n[DEBUG] Processing node %d: offset=%u, len=%u, RLE=%d\n", node->node_id, node->offset, len,
               node->is_RLE);
#endif
        uint16_t code;
        uint8_t code_class;
        if (node->is_RLE) {
            // RLE case: encoded using code_class = 3 (bits '11') and RLE format
            /*if (node->repeat_seq_length > 8) {
                fprintf(stderr, "RLE repeat_seq_length too large: %u\n", node->repeat_seq_length);
                exit(EXIT_FAILURE);
            }*/

#ifdef DEBUG
            printf("[DEBUG] Writing RLE sequence (count=%u)\n", node->length_of_RLE);
#endif

            // Write compressed flag (1) then class (3 == 11b)
            SAFE_BITWRITE(writer, 1, 1, file_to_write, "c_flag");
#ifdef DEBUG
            printf("[DEBUG] ➤ Written 1 bit: 1 (compressed flag)\n");
            bitwriter_print_state(writer);
#endif
            SAFE_BITWRITE(writer, 3, 2, file_to_write, "code_class"); // RLE uses class code 11b
#ifdef DEBUG
            printf("[DEBUG] ➤ Written 2 bits: code class = %u\n", 3);
            bitwriter_print_state(writer);
#endif

            /*            // RLE metadata
                        SAFE_BITWRITE(writer, node->repeat_seq_length, 3, file_to_write, "seq_len");
            #ifdef DEBUG
                        printf("[DEBUG] ➤ Written 3 bits: RLE repeat length = %u\n", node->repeat_seq_length);
                        bitwriter_print_state(writer);
            #endif
            */
            SAFE_BITWRITE(writer, node->length_of_RLE, 8, file_to_write, "len_of_RLE");
#ifdef DEBUG
            printf("[DEBUG] ➤ Written 8 bits: RLE count = %u\n", node->length_of_RLE);
            bitwriter_print_state(writer);
#endif

            // for (uint8_t j = 0; j < node->repeat_seq_length; ++j) {
            uint8_t j = 0;
            SAFE_BITWRITE(writer, seq[j], 8, file_to_write, "");
#ifdef DEBUG
            printf("[DEBUG] ➤ Written 8 bits: RLE pattern byte %02X\n", seq[j]);
            bitwriter_print_state(writer);
#endif
            //}
        } else if (len > 1 && code_map_get(&code_map, seq, len, &code, &code_class)) {
            // Compressed regular sequence - write compressed flag, class, and code index.
#ifdef DEBUG
            printf("[DEBUG] Writing compressed sequence (code=%u, class=%u)\n", code, code_class);
#endif

            SAFE_BITWRITE(writer, 1, 1, file_to_write, "c_flag");
#ifdef DEBUG
            printf("[DEBUG] ➤ Written 1 bit: 1 (compressed flag)\n");
            bitwriter_print_state(writer);
#endif

            SAFE_BITWRITE(writer, code_class, 2, file_to_write, "code_class");
#ifdef DEBUG
            printf("[DEBUG] ➤ Written 2 bits: code class = %u\n", code_class);
            bitwriter_print_state(writer);
#endif

            // Use class2 bit-width if code_class == 2, else fixed sizes from code_classes
            uint8_t bits_for_code = get_code_class_size(code_class, global_class2_bits);
            SAFE_BITWRITE(writer, code, bits_for_code, file_to_write, "code");
#ifdef DEBUG
            printf("[DEBUG] ➤ Written %u bits: code = %u\n", bits_for_code, code);
            bitwriter_print_state(writer);
#endif
        } else {
            // Uncompressed bytes: each raw byte prefixed with a '0' flag bit.
#ifdef DEBUG
            printf("[DEBUG] Writing uncompressed bytes (len=%u)\n", len);
#endif
            for (uint8_t j = 0; j < len; ++j) {
                SAFE_BITWRITE(writer, 0, 1, file_to_write, "seq_len");
#ifdef DEBUG
                printf("[DEBUG] ➤ Written 1 bit: 0 (uncompressed flag)\n");
                bitwriter_print_state(writer);
#endif
                SAFE_BITWRITE(writer, seq[j], 8, file_to_write, "");
#ifdef DEBUG
                printf("[DEBUG] ➤ Written 8 bits: raw byte %02X\n", seq[j]);
                bitwriter_print_state(writer);
#endif
            }
        }
    }

#ifdef DEBUG
    printf("[DEBUG] Body writing complete, flushing...\n");
#endif
    // Record file offset before flush
    long body_start = ftell(file_to_write);

    if (!bitwriter_write_to_file(writer, file_to_write)) {
        fprintf(stderr, "Failed to write final body data to file\n");
        exit(EXIT_FAILURE);
    }
    // Record file offset after flush
    long body_end = ftell(file_to_write);

    printf("Body size written: %ld bytes\n", body_end - body_start);

#ifdef DEBUG
    printf("[DEBUG] Body written successfully\n");
    bitwriter_print_state(writer);
#endif
}
