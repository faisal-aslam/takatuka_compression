// files/compression/compressed_body.c

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

long populate_body(BestPathView best_path, const uint8_t *block, FILE *file_to_write, BitWriter *writer) {
#ifdef DEBUG
    printf("\n\n\n******************************* \n\n[DEBUG] Starting body population with path size: %d\n",
           best_path.path_size);
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
        printf("\n\n[DEBUG] Processing node %d: offset=%u, len=%u, RLE_type=%d\n",
               node->node_id, node->offset, len, node->RLE_type);
#endif

        uint16_t code;
        uint8_t code_class;

        /* --- Uncompressed bytes --- */
        if (!(len > 1 && code_map_get(&code_map, seq, len, &code, &code_class)) && node->RLE_type == 0) {
            /* Not compressed and not RLE: write each raw byte with leading 0 bit */
#ifdef DEBUG
            printf("[DEBUG] Writing %u raw bytes (flag=0 each)\n", len);
#endif
            for (uint8_t j = 0; j < len; ++j) {
                SAFE_BITWRITE(writer, 0, 1, file_to_write, "raw_flag");     // 0 => uncompressed
                SAFE_BITWRITE(writer, seq[j], 8, file_to_write, "raw_byte"); // 8-bit byte
#ifdef DEBUG
                printf("[DEBUG] ➤ Raw byte %02X written\n", seq[j]);
                bitwriter_print_state(writer);
#endif
            }
            continue;
        }

        /* --- Compressed (flag = 1) --- */
        SAFE_BITWRITE(writer, 1, 1, file_to_write, "compressed_flag"); // 1 => compressed
#ifdef DEBUG
        printf("[DEBUG] ➤ Written 1 bit: 1 (compressed flag)\n");
        bitwriter_print_state(writer);
#endif

        /* If node is RLE -> class = 3. Otherwise class comes from code_map_get. */
        if (node->RLE_type) {
            /* RLE sequences encoded with class 3 (bits '11') */
            SAFE_BITWRITE(writer, 3, 2, file_to_write, "code_class"); // 2 bits: 11
#ifdef DEBUG
            printf("[DEBUG] ➤ Written code_class = 3 (RLE)\n");
            bitwriter_print_state(writer);
#endif

            if (node->RLE_type == 1) {
                /* Uniform RLE: write rle_flag = 1, then count (global_rle_bits), then symbol */
#ifdef DEBUG
                printf("[DEBUG] Writing Uniform RLE: count=%u, global_rle_bits=%u, symbol=%02X\n",
                       node->length_of_RLE, global_rle_bits, seq[0]);
#endif
                SAFE_BITWRITE(writer, 1, 1, file_to_write, "rle_flag_uniform"); // flag=1 => uniform RLE

                /* write length - use minimal bits computed in header, fallback to 8 if 0 */
                uint8_t rle_bits = (global_rle_bits > 0) ? global_rle_bits : 8;
                SAFE_BITWRITE(writer, node->length_of_RLE, rle_bits, file_to_write, "len_of_RLE");

                /* write symbol byte */
                SAFE_BITWRITE(writer, seq[0], 8, file_to_write, "RLE_symbol");

#ifdef DEBUG
                printf("[DEBUG] ➤ Uniform RLE written: count=%u symbol=%02X\n",
                       node->length_of_RLE, seq[0]);
                bitwriter_print_state(writer);
#endif
            } else if (node->RLE_type == 2) {
                /* Arithmetic RLE: write rle_flag = 0, then start and end bytes */
#ifdef DEBUG
                printf("[DEBUG] Writing Arithmetic RLE: start=%02X end=%02X\n", node->RLE_start, node->RLE_end);
#endif
                SAFE_BITWRITE(writer, 0, 1, file_to_write, "rle_flag_arith"); // flag=0 => arithmetic RLE

                SAFE_BITWRITE(writer, node->RLE_start, 8, file_to_write, "arithRLE_start");
                SAFE_BITWRITE(writer, node->RLE_end, 8, file_to_write, "arithRLE_end");

#ifdef DEBUG
                printf("[DEBUG] ➤ Arithmetic RLE written: start=%02X end=%02X\n", node->RLE_start, node->RLE_end);
                bitwriter_print_state(writer);
#endif
            } else {
                fprintf(stderr, "Unknown node->RLE_type=%u encountered in populate_body\n", node->RLE_type);
                exit(EXIT_FAILURE);
            }

        } else {
            /* Normal compressed code (class 0/1/2) */
#ifdef DEBUG
            printf("[DEBUG] Writing compressed sequence (code=%u, class=%u)\n", code, code_class);
#endif
            SAFE_BITWRITE(writer, code_class, 2, file_to_write, "code_class");
#ifdef DEBUG
            printf("[DEBUG] ➤ Written 2 bits: code class = %u\n", code_class);
            bitwriter_print_state(writer);
#endif

            uint8_t bits_for_code = get_code_class_size(code_class, global_class2_bits);
            SAFE_BITWRITE(writer, code, bits_for_code, file_to_write, "code");
#ifdef DEBUG
            printf("[DEBUG] ➤ Written %u bits: code = %u\n", bits_for_code, code);
            bitwriter_print_state(writer);
#endif
        }
    }

#ifdef DEBUG
    printf("[DEBUG] Body writing complete, flushing...\n");
#endif

    long body_start = ftell(file_to_write);

    if (!bitwriter_write_to_file(writer, file_to_write)) {
        fprintf(stderr, "Failed to write final body data to file\n");
        exit(EXIT_FAILURE);
    }
    long body_end = ftell(file_to_write);

    printf("Body size written: %ld bytes\n", body_end - body_start);

#ifdef DEBUG
    bitwriter_print_state(writer);
#endif

    return body_end - body_start;
}
