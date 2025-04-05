#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../weighted_freq.h"
#include "../second_pass/second_pass.h"

#define BUFFER_SIZE (1024 * 1024)  // 1MB buffer for better I/O performance
#define MAX_SEQ_LENGTH 8
#define ALIGNMENT 64  // Cache line alignment for AVX/SSE

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
 *    - Then the actual codeword (groupCodeSize(group) - 3 bits)
 *    - Note: groupCodeSize() includes 3 overhead bits (flag+group)
 *    - Example: Group 1 (7 bits total: 1 flag + 2 group + 4 codeword)
 * 
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
            // Process uncompressed sequence
            for (int j = 0; j < seq_len; j++) {
                write_bit(0, &bit_buffer, &bit_pos, file, byte_buffer, &byte_pos);
                uint8_t byte = sequence[j];
                // Unrolled loop for better performance
                write_bit((byte >> 7) & 1, &bit_buffer, &bit_pos, file, byte_buffer, &byte_pos);
                write_bit((byte >> 6) & 1, &bit_buffer, &bit_pos, file, byte_buffer, &byte_pos);
                write_bit((byte >> 5) & 1, &bit_buffer, &bit_pos, file, byte_buffer, &byte_pos);
                write_bit((byte >> 4) & 1, &bit_buffer, &bit_pos, file, byte_buffer, &byte_pos);
                write_bit((byte >> 3) & 1, &bit_buffer, &bit_pos, file, byte_buffer, &byte_pos);
                write_bit((byte >> 2) & 1, &bit_buffer, &bit_pos, file, byte_buffer, &byte_pos);
                write_bit((byte >> 1) & 1, &bit_buffer, &bit_pos, file, byte_buffer, &byte_pos);
                write_bit(byte & 1, &bit_buffer, &bit_pos, file, byte_buffer, &byte_pos);
            }
        } else {
            // Process compressed sequence
            write_bit(1, &bit_buffer, &bit_pos, file, byte_buffer, &byte_pos);
            
            // Write group bits
            uint8_t group = bin_seq->group;
            write_bit((group >> 1) & 1, &bit_buffer, &bit_pos, file, byte_buffer, &byte_pos);
            write_bit(group & 1, &bit_buffer, &bit_pos, file, byte_buffer, &byte_pos);
            
            // Write codeword bits
            uint8_t code_size = groupCodeSize(group) - 3;
            uint16_t codeword = bin_seq->codeword;
            for (int k = code_size - 1; k >= 0; k--) {
                write_bit((codeword >> k) & 1, &bit_buffer, &bit_pos, file, byte_buffer, &byte_pos);
            }
        }
    }

    // Flush remaining data
    if (bit_pos > 0) {
        byte_buffer[byte_pos++] = bit_buffer;
    }
    if (byte_pos > 0) {
        fwrite(byte_buffer, 1, byte_pos, file);
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
 *    b) The binary sequence itself.
 *    c) A 2-bit field for the group number (max: 4 groups).
 *    d) The corresponding codeword (length determined by the group number).
 *
 * Performance Optimizations:
 * - Uses buffered I/O for faster file writing.
 * - Packs bitfields efficiently to reduce file size.
 * - Processes data in a single pass for minimal overhead.
 *
 * @param topBinarySeq      Array of binary sequences.
 * @param length_of_seq     Number of sequences in the array.
 * @param output_file_name  Name of the output file.
 */
static void writeHeaderOfCompressedFile(BinarySequence* sequences, int count, FILE* file) {
    if (!sequences || count <= 0 || !file) {
        fprintf(stderr, "Error: Invalid inputs in writeHeaderOfCompressedFile\n");
        return;
    }

    // Count used sequences
    uint16_t used_count = 0;
    for (int i = 0; i < count; i++) {
        used_count += sequences[i].isUsed ? 1 : 0;
    }

    // Write count (big-endian for portability)
    uint8_t count_bytes[2] = {used_count >> 8, used_count & 0xFF};
    fwrite(count_bytes, 1, 2, file);

    uint8_t buffer[BUFFER_SIZE];
    size_t buf_pos = 0;

    for (int i = 0; i < count; i++) {
        if (!sequences[i].isUsed) continue;

        BinarySequence *seq = &sequences[i];
        uint8_t length = seq->length;
        uint8_t group = seq->group & 0x03;
        uint8_t code_size = groupCodeSize(group);

        if (length > MAX_SEQ_LENGTH) {
            fprintf(stderr, "Error: Sequence length %d exceeds maximum\n", length);
            continue;
        }

        // Prepare header byte
        uint8_t header = (length << 5) | (group << 3);

        // Check buffer space
        size_t needed = 1 + length + (code_size + 7) / 8;
        if (buf_pos + needed > BUFFER_SIZE) {
            fwrite(buffer, 1, buf_pos, file);
            buf_pos = 0;
        }

        // Write to buffer
        buffer[buf_pos++] = header;
        memcpy(buffer + buf_pos, seq->sequence, length);
        buf_pos += length;
        
        // Write codeword (handles variable code sizes)
        uint16_t codeword = seq->codeword;
        for (int j = (code_size - 1) / 8; j >= 0; j--) {
            buffer[buf_pos++] = (codeword >> (8 * j)) & 0xFF;
        }
    }

    // Flush remaining data
    if (buf_pos > 0) {
        fwrite(buffer, 1, buf_pos, file);
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
 void writeCompressedOutput(const char* filename, BinarySequence* sequences, 
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
 
     writeHeaderOfCompressedFile(sequences, seq_count, file);
     writeCompressedDataInFile(best_node, raw_data, file);
     
     if (fclose(file) != 0) {
         perror("Warning: Error closing output file");
     }
 }
