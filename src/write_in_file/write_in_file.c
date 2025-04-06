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
                     uint8_t* byte_buffer, size_t* byte_pos,
                     FILE* file);
                     
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
 *    - Followed by 2-bit group ID (values 1-4)
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
static void writeCompressedDataInFile(TreeNode *node, uint8_t* block, FILE *file) {
    if (!node || !block || !file) {
        fprintf(stderr, "Error: Invalid inputs in writeCompressedDataInFile\n");
        return;
    }
	if(1) return;
    uint8_t bit_buffer = 0;
    uint8_t bit_pos = 0;
    uint8_t* byte_buffer = create_aligned_buffer();
    size_t byte_pos = 0;

    uint32_t block_pos = 0;
    for (int i = 0; i < node->compress_sequence_count; i++) {
        uint16_t seq_len = node->compress_sequence[i];
        
        if (seq_len == 0 || seq_len > SEQ_LENGTH_LIMIT) {
            fprintf(stderr, "Warning: Skipping invalid sequence length %d\n", seq_len);
            continue;
        }

        uint8_t sequence[SEQ_LENGTH_LIMIT];
        if (block_pos + seq_len > BLOCK_SIZE) {
            fprintf(stderr, "Error: Block overflow at position %u\n", block_pos);
            break;
        }
        memcpy(sequence, block + block_pos, seq_len);
        block_pos += seq_len;

        BinarySequence* bin_seq = lookupSequence(sequence, seq_len);
        
	
		if (!bin_seq) {
			#ifdef DEBUG
			// Debug: Uncompressed sequence
			printf("\n[DEBUG] UNCOMPRESSED SEQUENCE (len=%d): ", seq_len);
			for (int j = 0; j < seq_len; j++) {
				printf("%02X ", sequence[j]);
			}
			printf("\n");

			// Write flag bit (0)
			printf("[DEBUG] Writing UNCOMPRESSED flag bit: 0\n");
 			#endif 
 			
			write_bit(0, &bit_buffer, &bit_pos, file, byte_buffer, &byte_pos);
			
			for (int j = 0; j < seq_len; j++) {
				printf("[DEBUG] Writing byte %02X bits: ", sequence[j]);
				for (int b = 7; b >= 0; b--) {
				    uint8_t bit = (sequence[j] >> b) & 1;
				    printf("%d", bit);
				    write_bit(bit, &bit_buffer, &bit_pos, file, byte_buffer, &byte_pos);
				}
				printf("\n");
			}
			
			// Debug buffer state
			printf("[DEBUG] After uncompressed write - bit_buffer=%02X, bit_pos=%d, byte_pos=%zu\n", 
				   bit_buffer, bit_pos, byte_pos);
		} else {
			// Debug: Compressed sequence
			printf("\n[DEBUG] COMPRESSED SEQUENCE (len=%d): ", bin_seq->length);
			for (int j = 0; j < bin_seq->length; j++) {
				printf("%02X ", bin_seq->sequence[j]);
			}
			printf(" | group=%d, codeword=%d (0x%X)\n", 
				   bin_seq->group, bin_seq->codeword, bin_seq->codeword);

			// Write flag bit (1)
			printf("[DEBUG] Writing COMPRESSED flag bit: 1\n");
			write_bit(1, &bit_buffer, &bit_pos, file, byte_buffer, &byte_pos);
			
			// Write group bits
			uint8_t group_bit1 = (bin_seq->group >> 1) & 1;
			uint8_t group_bit0 = bin_seq->group & 1;
			printf("[DEBUG] Writing GROUP bits (%d): %d%d\n", 
				   bin_seq->group, group_bit1, group_bit0);
			write_bit(group_bit1, &bit_buffer, &bit_pos, file, byte_buffer, &byte_pos);
			write_bit(group_bit0, &bit_buffer, &bit_pos, file, byte_buffer, &byte_pos);
			
			// Write codeword bits
			uint8_t code_size = groupCodeSize(bin_seq->group);
			printf("[DEBUG] Writing CODEWORD (%d bits): ", code_size);
			for (int k = code_size - 1; k >= 0; k--) {
				uint8_t bit = (bin_seq->codeword >> k) & 1;
				printf("%d", bit);
				write_bit(bit, &bit_buffer, &bit_pos, file, byte_buffer, &byte_pos);
			}
			printf("\n");
			
			// Debug buffer state
			printf("[DEBUG] After compressed write - bit_buffer=%02X, bit_pos=%d, byte_pos=%zu\n", 
				   bit_buffer, bit_pos, byte_pos);
			
			// Debug full byte buffer when it fills up
			if (byte_pos > 0 && (byte_pos % 16 == 0 || byte_pos == BUFFER_SIZE-1)) {
				printf("[DEBUG] Byte buffer contents (%zu bytes):\n", byte_pos);
				for (size_t i = 0; i < byte_pos; i++) {
				    printf("%02X ", byte_buffer[i]);
				    if ((i+1) % 16 == 0) printf("\n");
				}
				printf("\n");
			}
		}
				
		
		#ifdef DEBUG
		printf("Writing %s sequence: ", bin_seq ? "compressed" : "uncompressed");
		if (bin_seq) {
			printf("group=%d, codeword=0x%X\n", bin_seq->group, bin_seq->codeword);
		} else {
			printf("bytes: ");
			for (int j = 0; j < seq_len; j++) {
				printf("0x%02X ", sequence[j]);
			}
			printf("\n");
		}
		#endif

    }


	if (bit_pos > 0) { // Flush remaining bits
		byte_buffer[byte_pos++] = bit_buffer;
		bit_buffer = 0;
		bit_pos = 0;
	}
    
    free(byte_buffer);
}


/**
 * @brief Writes all used binary sequences (`isUsed = 1`) to a file in a compressed format.
 *
 * The file structure is as follows:
 * 1) The first two bytes store the total count of used sequences.
 * 2) Each sequence entry contains:
 *    a) A 3-bit field representing the length of the sequence (max: 8 bytes).
 *    b) The binary sequence itself (from 1 to 8 bytes of length depending upon length field)
 *    c) A 2-bit field for the group number (max: 4 groups).
 *    d) The corresponding codeword (length determined by the group number using function groupCodeSize(group))
 * 		Note, codeword could be a few bit long to a many bytes long. Thus, we have to be careful here.
 *  [16-bit sequence count] Then for each sequence:
                           [3-bit length][length×8-bit sequence][2-bit group][variable-bit codeword]
 * Performance Optimizations:
 * - Uses buffered I/O for faster file writing.
 * - Packs bitfields efficiently to reduce file size.
 * - Processes data in a single pass for minimal overhead.
 *
 * @param topBinarySeq      Array of binary sequences.
 * @param seq_count     Number of sequences in the array.
 * @param output_file_name  Name of the output file.
 */
static void writeHeaderOfCompressedFile(BinarySequence** topBinarySeq, int seq_count,
                                      uint16_t used_seq_count, FILE* file) {
    if (!topBinarySeq || seq_count <= 0 || !file) {
        fprintf(stderr, "Error: Invalid inputs\n");
        return;
    }

    // 1. Write 2-byte sequence count (big-endian)
    uint8_t count_bytes[2] = {used_seq_count >> 8, used_seq_count & 0xFF};
    if (fwrite(count_bytes, 1, 2, file) != 2) {
        fprintf(stderr, "Error writing sequence count\n");
        return;
    }

    uint8_t bit_buffer = 0;
    uint8_t bit_pos = 0;
    uint8_t byte_buffer[BUFFER_SIZE];
    size_t byte_pos = 0;

    // NEW: Debug header
    printf("\n=== Writing Header ===\n");
    printf("Total sequences: %d\n", used_seq_count);

    for (int i = 0; i < seq_count; i++) {
        if (!topBinarySeq[i] || !topBinarySeq[i]->isUsed) continue;

        BinarySequence *seq = topBinarySeq[i];
        uint8_t code_size = groupCodeSize(seq->group);

        // NEW: Debug before writing each sequence
        printf("\nSequence %d: ", i);
        for (int j = 0; j < seq->length; j++) printf("%02X ", seq->sequence[j]);
        printf("(len:%d group:%d codeword:%d)\n", 
               seq->length, seq->group, seq->codeword);

        // 2. Write 3-bit length (MSB first)
        write_bits(seq->length, 3, &bit_buffer, &bit_pos, byte_buffer, &byte_pos, file);

        // 3. Write sequence bytes (MSB first)
        for (int j = 0; j < seq->length; j++) {
            write_bits(seq->sequence[j], 8, &bit_buffer, &bit_pos, byte_buffer, &byte_pos, file);
        }

        // 4. Write 2-bit group (MSB first)
        write_bits(seq->group, 2, &bit_buffer, &bit_pos, byte_buffer, &byte_pos, file);

        // 5. Write codeword (MSB first)
        write_bits(seq->codeword, code_size, &bit_buffer, &bit_pos, byte_buffer, &byte_pos, file);

        // Flush if buffer is nearly full
        if (byte_pos >= BUFFER_SIZE - 4) {
            fwrite(byte_buffer, 1, byte_pos, file);
            byte_pos = 0;
        }
    }

    // NEW: Debug before final write
    printf("\nFinal bit buffer: %02X (%d bits)\n", bit_buffer, bit_pos);
    
    // Write only complete bytes (leave remaining bits for data section)
    if (byte_pos > 0) {
        printf("Writing %zu bytes: ", byte_pos);
        for (size_t i = 0; i < byte_pos; i++) printf("%02X ", byte_buffer[i]);
        printf("\n");
        
        fwrite(byte_buffer, 1, byte_pos, file);
    }
}

// NEW: Helper function for bit-perfect writing
static void write_bits(uint16_t data, uint8_t num_bits,
                      uint8_t* bit_buffer, uint8_t* bit_pos,
                      uint8_t* byte_buffer, size_t* byte_pos,
                      FILE* file) {
    // NEW: Debug output
    printf("Writing %d bits of %04X: ", num_bits, data);
    for (int b = num_bits - 1; b >= 0; b--) printf("%d", (data >> b) & 1);
    printf("\n");

    for (int b = num_bits - 1; b >= 0; b--) {
        *bit_buffer |= ((data >> b) & 1) << (7 - *bit_pos);
        if (++(*bit_pos) == 8) {
            byte_buffer[(*byte_pos)++] = *bit_buffer;
            printf("  -> Byte complete: %02X\n", *bit_buffer);
            *bit_buffer = 0;
            *bit_pos = 0;
            if (*byte_pos == BUFFER_SIZE) {
                fwrite(byte_buffer, 1, BUFFER_SIZE, file);
                *byte_pos = 0;
            }
        }
    }
}



/**
 * @brief Get the overhead bits for a group (flag + group bits)
 * 
 * @param group The group number (1-4)
 * @return uint8_t The 3-bit overhead (1 flag bit + 2 group bits)
 * 
  * Group 1: 100 (binary) - 0x4
  * Group 2: 101 (binary) - 0x5
  * Group 3: 110 (binary) - 0x6
  * Group 4: 111 (binary) - 0x7
  */
static uint8_t groupCodeOverhead(uint8_t group) {
    switch(group) {
        case 1: return 0x4;  // 100 (binary)
        case 2: return 0x5;  // 101 (binary)
        case 3: return 0x6;  // 110 (binary)
        case 4: return 0x7;  // 111 (binary)
        default:
            fprintf(stderr, "Error: Invalid group %d in groupCodeOverhead\n", group);
            exit(EXIT_FAILURE);
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
    uint16_t next_codeword[TOTAL_GROUPS + 1] = {0};  // Groups 1-4
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
            if (bin_seq->group < 1 || bin_seq->group > TOTAL_GROUPS) {
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
            
            #ifdef DEBUG
            printf("Assigned: Seq[");
            for (int k = 0; k < seq_len; k++) printf("%02X ", sequence[k]);
            printf("] -> Group %d, Codeword %d\n", bin_seq->group, bin_seq->codeword);
            #endif
        }
    }
    
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
    printf("\n In Write compressed Output. Going to print the best node \n");
	//printNode(best_node, raw_data, 0);
    int used_count = calcUsedAndAssignGroupID(best_node, raw_data, 0);
    writeHeaderOfCompressedFile(sequences, seq_count, used_count, file);
    /////writeCompressedDataInFile(best_node, raw_data, file);
   
    if (fclose(file) != 0) {
        perror("Warning: Error closing output file");
    }
}

