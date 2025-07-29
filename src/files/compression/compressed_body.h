// compressed_body.h

#pragma once

#include "best_path_view.h"
#include "bit_writer.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Unified safe writing macro for both header and body
#ifdef DEBUG
#define SAFE_BITWRITE(bw, value, bits, file, bitstr)                                                                   \
    do {                                                                                                               \
        if (!bitwriter_write(bw, value, bits, bitstr)) {                                                               \
            if (!bitwriter_write_to_file(bw, file)) {                                                                  \
                fprintf(stderr, "Failed to write buffer to file\n");                                                   \
                exit(EXIT_FAILURE);                                                                                    \
            }                                                                                                          \
            bitwriter_reset(bw);                                                                                       \
            if (!bitwriter_write(bw, value, bits, bitstr)) {                                                           \
                fprintf(stderr, "bitwriter_write failed after flush (DEBUG)\n");                                       \
                exit(EXIT_FAILURE);                                                                                    \
            }                                                                                                          \
        }                                                                                                              \
    } while (0)
#else
#define SAFE_BITWRITE(bw, value, bits, file, bitstr_unused)                                                            \
    do {                                                                                                               \
        if (!bitwriter_write(bw, value, bits)) {                                                                       \
            if (!bitwriter_write_to_file(bw, file)) {                                                                  \
                fprintf(stderr, "Failed to write buffer to file\n");                                                   \
                exit(EXIT_FAILURE);                                                                                    \
            }                                                                                                          \
            bitwriter_reset(bw);                                                                                       \
            if (!bitwriter_write(bw, value, bits)) {                                                                   \
                fprintf(stderr, "bitwriter_write failed after flush\n");                                               \
                exit(EXIT_FAILURE);                                                                                    \
            }                                                                                                          \
        }                                                                                                              \
    } while (0)
#endif



/**
 * @brief Processes BestPathView and writes compressed data to output file
 *
 * Full processing logic:
Processing Logic:

1. Reverse Processing:
   - Traverse the BestPathView in reverse order (from end to start)

2. Node Handling:
   - Skip root nodes (where node_id == 0)

3. Uncompressed Nodes (leaf cases):
   - For nodes with sequence_length == 1 OR frequency == 1:
     * For EACH byte in the node:
       - Write a '0' prefix bit (indicates uncompressed data)
       - Write the raw byte (total: 9 bits per byte - 1 flag + 8 data bits)

4. Compressed Non-RLE Nodes:
   - For nodes with sequence_length > 1 AND frequency > 1 AND is_RLE == false:
     * Verify sequence exists in code_map (abort if missing)
     * Write a '1' prefix bit (indicates compressed data)
     * Write 2-bit code_class identifier
     * Determine code length using get_code_class_size(code_class)
     * Write the corresponding code from code_map using the determined bit length

5. RLE Nodes:
   - For nodes with is_RLE == true:
     * Write a '1' prefix bit (indicates compressed data)
     * Verify repeat_seq_length ≤ 8 (abort if larger)
     * Write 3-bit repeat_seq_length value
     * Verify length_of_RLE ≤ 255 (abort if larger)
     * Write 1-byte length_of_RLE value (comes BEFORE the sequence)
     * Write the initial repeat_seq_length bytes from the node's sequence


Error Handling:
- Abort processing with error message if:
  * A compressible sequence is missing from code_map
  * RLE repeat_seq_length > 8
  * RLE length_of_RLE > 255

Important Notes:
1. For uncompressed bytes:
   - Each individual byte gets its own '0' prefix bit
   - Example: A 3-byte uncompressed node becomes: [0 b1][0 b2][0 b3]

2. For RLE nodes:
   - New format: 1 (flag) + 3 (repeat len) + 8 (RLE count) + sequence bytes
   - This allows the decoder to first know how many times to repeat before seeing what to repeat

3. Bit Alignment:
   - All writes must maintain proper bit packing across byte boundaries
   - May need to track partial bytes during writing


 *
 * @param best_path The optimal compression path
 * @param block Input data block
 * @param file_to_write Output file handle
 * @param writer BitWriter instance to use
 */
void populate_body(BestPathView best_path, const uint8_t *block, FILE *file_to_write, BitWriter *writer);
