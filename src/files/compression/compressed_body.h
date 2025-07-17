// compressed_body.h

#pragma once

#include <stdint.h>
#include <stdio.h>  
#include <string.h>
#include "best_path_view.h"
#include "bit_writer.h"

// Unified safe writing macro for both header and body
#define SAFE_BITWRITE(bw, value, bits, file)                   \
    do {                                                        \
        if (!bitwriter_write(bw, value, bits)) {                \
            bitwriter_flush(bw);                               \
            if (!bitwriter_write_to_file(bw, file)) {           \
                fprintf(stderr, "Failed to write buffer to file\n"); \
                exit(EXIT_FAILURE);                            \
            }                                                  \
            bitwriter_reset(bw);                               \
            if (!bitwriter_write(bw, value, bits)) {            \
                fprintf(stderr, "bitwriter_write failed after flush\n"); \
                exit(EXIT_FAILURE);                            \
            }                                                   \
        }                                                       \
    } while (0)

/**
 * @brief Processes BestPathView and writes compressed data to output file
 * 
 * Full processing logic:
 * 1. Reverse traversal of BestPathView
 * 2. Handles uncompressed nodes (prefix 0 + raw byte for each byte)
 * 3. Handles compressed non-RLE nodes (prefix 1 + code class + code)
 * 4. Handles RLE nodes (prefix 1 + repeat len + RLE count + sequence)
 * 
 * 
 * @param best_path The optimal compression path
 * @param block Input data block
 * @param file_to_write Output file handle
 * @param writer BitWriter instance to use
 */
void populate_body(BestPathView best_path, const uint8_t* block, 
                  FILE* file_to_write, BitWriter* writer);
