#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h> 
#include "../weighted_freq.h"
#include "../second_pass/second_pass.h"

#define BUFFER_SIZE (1024 * 1024)  // 1MB buffer for better I/O performance
#define MAX_SEQ_LENGTH 8
#define ALIGNMENT 64  // Cache line alignment for AVX/SSE

static void write_bits(uint16_t data, uint8_t num_bits,
                      uint8_t* bit_buffer, uint8_t* bit_pos,
                      uint8_t* byte_buffer, size_t* byte_pos);
                     
/**
 * @brief Creates an aligned buffer for optimal memory access
 */
static uint8_t* create_aligned_buffer() {
    uint8_t* buf = aligned_alloc(ALIGNMENT, BUFFER_SIZE);
    if (!buf) {
        fprintf(stderr, "Error: Failed to allocate aligned buffer\n");
        exit(EXIT_FAILURE);
    }
    return buf;
}

/**
 * @brief Writes individual bits with aligned buffers and optimized flushing
 * 
 * @param bit The bit value (0 or 1) to write
 * @param buffer Pointer to the current byte buffer
 * @param bit_pos Pointer to current bit position (0-7)
 * @param file Output file handle
 * @param byte_buffer Main data buffer for bulk writes
 * @param byte_pos Pointer to current position in byte_buffer
 * 
 * @note This function automatically flushes the byte_buffer when full
 */
static inline void write_bit(uint8_t bit, uint8_t* buffer, uint8_t* bit_pos, 
                           FILE* file, uint8_t* byte_buffer, size_t* byte_pos) {
    *buffer |= (bit & 1) << (7 - *bit_pos);
    (*bit_pos)++;
    
    if (*bit_pos == 8) {
        byte_buffer[(*byte_pos)++] = *buffer;
        *buffer = 0;
        *bit_pos = 0;
        
        // Flush aligned buffer when full
        if (*byte_pos == BUFFER_SIZE) {
            fwrite(byte_buffer, 1, BUFFER_SIZE, file);
            *byte_pos = 0;
        }
    }
}



 /**
 * @brief Writes compressed data to a file with proper flagging and bit-level organization.
 * 
 * This function processes compressed sequences from a TreeNode and writes them to a file
 * with precise bit-level formatting to distinguish between compressed and uncompressed data.
 * 
 * Data Format Rules:
 * 1. For uncompressed sequences:
 *    - Each byte is preceded by a '0' flag (1 bit)
 *    - Format: [0][8 bits of raw data] per byte
 *    - Example: "AB" → 0 01000001 0 01000010 (18 bits total)
 * 
 * 2. For compressed sequences:
 *    - Sequence starts with '1' flag (1 bit)
 *    - Followed by 2-bit group ID (values 0-3)
 *    - Then the actual codeword (groupCodeSize(group))
 * @param node The TreeNode containing compression metadata with:
 *             - compress_sequence: Array of sequence lengths
 *             - compress_sequence_count: Valid entries in compress_sequence
 * @param block Source data block containing raw bytes to compress
 * @param file Output file handle (must be open for binary writing)
 * 
 * @note Important Implementation Details:
 *       - Handles bit-level writing with proper alignment
 *       - Validates all inputs before processing
 *       - Skips invalid sequences gracefully
 *       - Maintains accurate bit counts for both compressed and raw data
 * 
 * @example 
 *   Uncompressed "AB" (2 bytes):
 *     Writes: 0 01000001 0 01000010 (18 bits)
 *   Compressed sequence (group 1):
 *     Writes: 1 01 [4-bit codeword] (7 bits total)
*/
/**
 * @brief Writes compressed data to a file with extensive debugging output
 */
static void writeCompressedDataInFile(TreeNode *node, uint8_t* block, FILE *file) {
    if (!node || !block || !file) {
        fprintf(stderr, "Error: Invalid inputs in writeCompressedDataInFile\n");
        return;
    }

    #ifdef DEBUG
    printf("\n=== Starting writeCompressedDataInFile ===\n");
    printf("Node has %d sequences to process\n", node->compress_sequence_count);
    #endif

    uint8_t bit_buffer = 0;
    uint8_t bit_pos = 0;
    uint8_t* byte_buffer = create_aligned_buffer();
    size_t byte_pos = 0;

    uint32_t block_pos = 0;
    for (int i = 0; i < node->compress_sequence_count; i++) {
        uint16_t seq_len = node->compress_sequence[i];
        
        #ifdef DEBUG
        printf("\n[Sequence %d] Start processing (len=%d)\n", i, seq_len);
        #endif

        if (seq_len == 0 || seq_len > SEQ_LENGTH_LIMIT) {
            #ifdef DEBUG
            fprintf(stderr, "Warning: Skipping invalid sequence length %d\n", seq_len);
            #endif
            continue;
        }

        uint8_t sequence[SEQ_LENGTH_LIMIT];
        if (block_pos + seq_len > BLOCK_SIZE) {
            #ifdef DEBUG
            fprintf(stderr, "Error: Block overflow at position %u\n", block_pos);
            #endif
            break;
        }
        memcpy(sequence, block + block_pos, seq_len);
        block_pos += seq_len;

        BinarySequence* bin_seq = lookupSequence(sequence, seq_len);

        #ifdef DEBUG
        printf("Current bit state: buffer=%02X pos=%d\n", bit_buffer, bit_pos);
        #endif

        if (!bin_seq) {
            #ifdef DEBUG
            printf("[UNCOMPRESSED] Seq len %d: ", seq_len);
            for (int j = 0; j < seq_len; j++) printf("%02X ", sequence[j]);
            printf("\n");
            printf("Writing flag bit 0\n");
            #endif
            
            write_bit(0, &bit_buffer, &bit_pos, file, byte_buffer, &byte_pos);
            
            for (int j = 0; j < seq_len; j++) {
                #ifdef DEBUG
                printf("Byte %d/%d (%02X): ", j+1, seq_len, sequence[j]);
                #endif
                for (int b = 7; b >= 0; b--) {
                    uint8_t bit = (sequence[j] >> b) & 1;
                    #ifdef DEBUG
                    printf("%d", bit);
                    #endif
                    write_bit(bit, &bit_buffer, &bit_pos, file, byte_buffer, &byte_pos);
                }
                #ifdef DEBUG
                printf(" | buffer=%02X pos=%d\n", bit_buffer, bit_pos);
                #endif
            }
        } else {
            #ifdef DEBUG
            printf("[COMPRESSED] Found in dictionary: ");
            for (int j = 0; j < bin_seq->length; j++) printf("%02X ", bin_seq->sequence[j]);
            printf("| group=%d codeword=%d (size=%d bits)\n", 
                  bin_seq->group, bin_seq->codeword, groupCodeSize(bin_seq->group));
            printf("Writing flag bit 1\n");
            #endif
            
            write_bit(1, &bit_buffer, &bit_pos, file, byte_buffer, &byte_pos);
            
            // Write group bits
            uint8_t group_bit1 = (bin_seq->group >> 1) & 1;
            uint8_t group_bit0 = bin_seq->group & 1;
            #ifdef DEBUG
            printf("Writing group %d as bits: %d%d\n", bin_seq->group, group_bit1, group_bit0);
            #endif
            write_bit(group_bit1, &bit_buffer, &bit_pos, file, byte_buffer, &byte_pos);
            write_bit(group_bit0, &bit_buffer, &bit_pos, file, byte_buffer, &byte_pos);
            
            // Write codeword
            uint8_t code_size = groupCodeSize(bin_seq->group);
            #ifdef DEBUG
            printf("Writing codeword %d (%d bits): ", bin_seq->codeword, code_size);
            #endif
            for (int k = code_size - 1; k >= 0; k--) {
                uint8_t bit = (bin_seq->codeword >> k) & 1;
                #ifdef DEBUG
                printf("%d", bit);
                #endif
                write_bit(bit, &bit_buffer, &bit_pos, file, byte_buffer, &byte_pos);
            }
            #ifdef DEBUG
            printf(" | buffer=%02X pos=%d\n", bit_buffer, bit_pos);
            #endif
        }

        #ifdef DEBUG
        // Show buffer state after each sequence
        printf("After sequence %d: bit_buffer=%02X bit_pos=%d byte_pos=%zu\n",
              i, bit_buffer, bit_pos, byte_pos);
        
        /* Show complete bytes when buffer reaches certain points
        if (byte_pos > 0 && (byte_pos % 16 == 0 || byte_pos > BUFFER_SIZE - 8)) {
            printf("Current byte buffer (%zu bytes):\n", byte_pos);
            for (size_t j = 0; j < byte_pos; j++) {
                printf("%02X ", byte_buffer[j]);
                if ((j+1) % 16 == 0) printf("\n");
            }
            printf("\n");
        }*/
        #endif
    }

    // Flush remaining bits
    if (bit_pos > 0) {
        #ifdef DEBUG
        printf("Flushing remaining bits: %02X (%d bits)\n", bit_buffer, bit_pos);
        #endif
        byte_buffer[byte_pos++] = bit_buffer;
    }

    #ifdef DEBUG
    printf("\n=== Final Output ===\n");
    printf("Total bytes to write: %zu\n", byte_pos);
    for (size_t i = 0; i < byte_pos; i++) printf("%02X ", byte_buffer[i]);
    printf("\n");
    #endif

    // Write final buffer
    if (byte_pos > 0) {
        fwrite(byte_buffer, 1, byte_pos, file);
    }
    
    free(byte_buffer);

    #ifdef DEBUG
    printf("=== writeCompressedDataInFile completed ===\n\n");
    #endif
}

#ifdef DEBUG
// Helper function to print binary representation of a byte
static void print_binary(uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        printf("%d", (byte >> i) & 1);
    }
}
#endif

/**
 * @brief Writes compressed header with bit-perfect packing and optional padding
 * 
 * Structure:
 * 1. 2-byte sequence count (big-endian)
 * 2. For each sequence:
 *    - 3-bit length
 *    - N bytes of sequence data
 *    - 2-bit group
 *    - Variable-length codeword
 * 
 * Note: Final byte is padded with zeros if needed for byte alignment
 */
static void writeHeaderOfCompressedFile(BinarySequence** topBinarySeq, int seq_count,
                                      uint16_t used_seq_count, FILE* file) {
    if (!topBinarySeq || seq_count <= 0 || !file) return;
    
    #ifdef DEBUG
    printf("=== Writing Header ===\n");
    printf("Total sequences: %d\n", used_seq_count);
    #endif
    
    // 1. Write sequence count (2 bytes big-endian)
    uint8_t count_bytes[2] = {used_seq_count >> 8, used_seq_count & 0xFF};
    fwrite(count_bytes, 1, 2, file);
    
    #ifdef DEBUG
    printf("Written count: %02X %02X\n", count_bytes[0], count_bytes[1]);
    #endif

    uint8_t bit_buffer = 0;
    uint8_t bit_pos = 0;
    uint8_t byte_buffer[BUFFER_SIZE];
    size_t byte_pos = 0;

    for (int i = 0; i < seq_count; i++) {
        if (!topBinarySeq[i] || !topBinarySeq[i]->isUsed) continue;

        BinarySequence *seq = topBinarySeq[i];
        uint8_t code_size = groupCodeSize(seq->group);
        
        #ifdef DEBUG
        printf("\nSequence %d: ", i);
        for (int j = 0; j < seq->length; j++) printf("%02X ", seq->sequence[j]);
        printf("(len:%d group:%d codeword:%d)\n", seq->length, seq->group, seq->codeword);
        #endif

        // 1. Write 3-bit length
        #ifdef DEBUG
        printf("Writing length (%d): ", seq->length);
        #endif
        write_bits(seq->length, 3, &bit_buffer, &bit_pos, byte_buffer, &byte_pos);
        
        #ifdef DEBUG
        printf("-> buffer: %02X, pos: %d\n", bit_buffer, bit_pos);
        #endif

        // 2. Write sequence bytes
        for (int j = 0; j < seq->length; j++) {
            #ifdef DEBUG
            printf("Writing byte %02X: ", seq->sequence[j]);
            #endif
            
            if (bit_pos == 0) {
                // Byte-aligned, write directly
                byte_buffer[byte_pos++] = seq->sequence[j];
                #ifdef DEBUG
                printf("(aligned) -> %02X\n", seq->sequence[j]);
                #endif
            } else {
                // Split across byte boundary
                byte_buffer[byte_pos++] = bit_buffer | (seq->sequence[j] >> bit_pos);
                bit_buffer = seq->sequence[j] << (8 - bit_pos);
                #ifdef DEBUG
                printf("(split) -> %02X + remaining %02X\n", 
                       byte_buffer[byte_pos-1], bit_buffer);
                #endif
            }
        }

        // 3. Write 2-bit group
        #ifdef DEBUG
        printf("Writing group (%d): ", seq->group);
        #endif
        write_bits(seq->group, 2, &bit_buffer, &bit_pos, byte_buffer, &byte_pos);
        
        #ifdef DEBUG
        printf("-> buffer: %02X, pos: %d\n", bit_buffer, bit_pos);
        #endif

        // 4. Write codeword
        #ifdef DEBUG
        printf("Writing codeword (%d, %d bits): ", seq->codeword, code_size);
        #endif
        write_bits(seq->codeword, code_size, &bit_buffer, &bit_pos, byte_buffer, &byte_pos);
        
        #ifdef DEBUG
        printf("-> buffer: %02X, pos: %d\n", bit_buffer, bit_pos);
        printf("Current bytes:");
        for (size_t k = 0; k < byte_pos; k++) printf(" %02X", byte_buffer[k]);
        printf("\n");
        #endif
    }

    // Finalize any remaining bits
    if (bit_pos > 0) {
        byte_buffer[byte_pos++] = bit_buffer;
        #ifdef DEBUG
        printf("Final padding: %02X (%d bits)\n", bit_buffer, bit_pos);
        #endif
    }

    // Add end marker
    byte_buffer[byte_pos++] = 0xFF;
    
    #ifdef DEBUG
    printf("\nFinal bytes (%zu):", byte_pos);
    for (size_t i = 0; i < byte_pos; i++) printf(" %02X", byte_buffer[i]);
    printf("\n");
    #endif

    fwrite(byte_buffer, 1, byte_pos, file);
}

/**
 * @brief Writes bits to buffer (MSB first)
 * 
 * @param data The value to write
 * @param num_bits How many bits to write (1-16)
 * @param bit_buffer Current bit accumulator
 * @param bit_pos Current bit position (0-7)
 * @param byte_buffer Main output buffer
 * @param byte_pos Position in output buffer
 */
static void write_bits(uint16_t value, uint8_t num_bits, uint8_t* bit_buffer, uint8_t* bit_pos,
                     uint8_t* byte_buffer, size_t* byte_pos) {
    for (int i = num_bits - 1; i >= 0; i--) {
        *bit_buffer |= ((value >> i) & 1) << (7 - *bit_pos);
        (*bit_pos)++;
        
        if (*bit_pos == 8) {
            byte_buffer[(*byte_pos)++] = *bit_buffer;
            *bit_buffer = 0;
            *bit_pos = 0;
        }
    }
}

/**
 * @brief Calculates used sequences and assigns group IDs/codewords
 * 
 * @param node TreeNode containing compression sequences
 * @param block Raw data block
 * @param block_index Starting index in block
 * @return int Number of used sequences found (or -1 on error)
 */
static int calcUsedAndAssignGroupID(TreeNode *node, uint8_t* block, uint32_t block_index) {
    // Validate inputs
    if (!node || !block) {
        fprintf(stderr, "Error: Invalid node or block pointer\n");
        return -1;
    }
    
    if (block_index >= BLOCK_SIZE) {
        fprintf(stderr, "Error: Block index out of bounds\n");
        return -1;
    }

    // Initialize codeword counters for each group
    uint16_t next_codeword[TOTAL_GROUPS] = {0};  // Groups 0-3
    int used_count = 0;

    // Process each sequence in the node
    for (int i = 0; i < node->compress_sequence_count && i < COMPRESS_SEQUENCE_LENGTH; i++) {
        uint16_t seq_len = node->compress_sequence[i];
        
        // Validate sequence length
        if (seq_len == 0 || seq_len > SEQ_LENGTH_LIMIT) {
            fprintf(stderr, "Warning: Skipping invalid sequence length %d at index %d\n", seq_len, i);
            continue;
        }

        // Check block bounds
        if (block_index + seq_len > BLOCK_SIZE) {
            fprintf(stderr, "Error: Sequence exceeds block bounds at index %d\n", i);
            return -1;
        }

        // Extract sequence from block
        uint8_t sequence[SEQ_LENGTH_LIMIT];
        memcpy(sequence, block + block_index, seq_len);
        block_index += seq_len;

        // Look up sequence in dictionary
        BinarySequence* bin_seq = lookupSequence(sequence, seq_len);
        if (!bin_seq) {
            continue;  // Sequence not in dictionary
        }

        // Mark as used and assign codeword
        if (!bin_seq->isUsed) {
            bin_seq->isUsed = 1;
            used_count++;
            
            // Validate group
            if (bin_seq->group < 0 || bin_seq->group > TOTAL_GROUPS-1) {
                fprintf(stderr, "Error: Invalid group %d for sequence\n", bin_seq->group);
                continue;
            }

            // Get codeword size for this group
            uint8_t code_size = groupCodeSize(bin_seq->group);
            uint16_t max_codeword = (1 << code_size) - 1;
            
            // Check codeword availability
            if (next_codeword[bin_seq->group] > max_codeword) {
                fprintf(stderr, "Error: No more codewords available for group %d\n", bin_seq->group);
                continue;
            }
            
            // Assign codeword
            bin_seq->codeword = next_codeword[bin_seq->group]++;
            
        }
    }
    
    #ifdef DEBUG
    printf("\n\n");
    for (int k = 0; k < TOTAL_GROUPS; k++) {
    	printf("\n group=%d, total_codewords=%d", k, next_codeword[k]); 
    }	
    printf("\n\n");
    #endif


    return used_count;
}


static void printNode(TreeNode *node, uint8_t* block, uint32_t block_index) {
    if (!node) {
        printf("[NULL NODE]\n");
        return;
    }
    
    // Use direct values but add bounds checking
    printf("\n Node :");
    printf("saving_so_far=%u, incoming_weight=%d, isPruned=%d, compress_seq_count=%d", 
           node->saving_so_far, 
           node->incoming_weight, 
           node->isPruned, 
           (node->compress_sequence_count < COMPRESS_SEQUENCE_LENGTH) ? node->compress_sequence_count : 0);
             
    // Bounds checking
    if (node->compress_sequence_count > COMPRESS_SEQUENCE_LENGTH) {
        printf("\n\t[ERROR: Invalid compress_sequence_count]");
        return;
    }
    
    for (int i=0; i < node->compress_sequence_count; i++) {
        printf("\n\tNode compressed = %d", node->compress_sequence[i]);
        printf("\t { ");
        for (int j=0; j < node->compress_sequence[i]; j++) {
            if (block && (block_index + j) < BLOCK_SIZE) {
                printf("0x%x", block[block_index + j]);
            } else {
                printf("[INVALID]");
            }
            if (j+1 < node->compress_sequence[i]) printf(", ");
        }
        printf(" }, ");
    }
}


/**
  * @brief Main function to write complete compressed output file
  * 
  * @param filename Output file path
  * @param sequences Array of binary sequences for header
  * @param seq_count Number of sequences
  * @param best_node TreeNode with best compression path
  * @param raw_data Pointer to raw data block
  */
void writeCompressedOutput(const char* filename, BinarySequence** sequences, 
                         int seq_count, TreeNode *best_node, uint8_t* raw_data) {
    if (!filename || !sequences || seq_count <= 0 || !best_node || !raw_data) {
        fprintf(stderr, "Error: Invalid inputs in writeCompressedOutput\n");
        return;
    }

    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open output file");
        return;
    }
    printf("\n ==== Starting compressed output writing === \n");
	//printNode(best_node, raw_data, 0);
    int used_count = calcUsedAndAssignGroupID(best_node, raw_data, 0);
    writeHeaderOfCompressedFile(sequences, seq_count, used_count, file);
    writeCompressedDataInFile(best_node, raw_data, file);
    printf("\n === Writing compressed output completed ==\n");
   
    if (fclose(file) != 0) {
        perror("Warning: Error closing output file");
    }
}

