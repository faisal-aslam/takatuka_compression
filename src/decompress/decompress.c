#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../constants.h"

typedef struct {
    uint8_t *sequence;   // The binary sequence
    int length;          // Length of the binary sequence
    uint8_t group;       // There could be at most GROUP_MAX groups 
    uint8_t isUsed;      // 1 if the sequence was used, otherwise 0
    uint16_t codeword;   // Codeword corresponding to the sequence
} BinarySequence;


static inline uint8_t groupCodeSize(uint8_t group) {
    switch(group) {
        case 1: return 4;  // 4-bit codeword
        case 2: return 4;  // 4-bit codeword
        case 3: return 4;  // 4-bit codeword
        case 4: return 12; // 12-bit codeword
        default:
            fprintf(stderr, "Invalid group %d\n", group);
            return 0;
    }
}


// Optimized buffer size (1MB) with 64-byte alignment for AVX/SSE
#define BUFFER_SIZE (1024 * 1024)

// Create 64-byte aligned buffer (replaces static arrays)
static uint8_t* create_aligned_buffer() {
    uint8_t* buf = aligned_alloc(64, BUFFER_SIZE);
    if (!buf) {
        fprintf(stderr, "Error: Failed to allocate aligned buffer\n");
        exit(EXIT_FAILURE);
    }
    return buf;
}

// Reads bits from a buffered input (LSB first)
static inline uint8_t read_bit(uint8_t* buffer, uint8_t* bit_pos,
                             FILE* file, uint8_t* byte_buffer,  // Changed back to single pointer
                             size_t* byte_pos, size_t* bytes_read) {
    // Refill buffer if needed
    if (*bit_pos == 0) {
        if (*byte_pos >= *bytes_read) {
            *bytes_read = fread(byte_buffer, 1, BUFFER_SIZE, file);
            *byte_pos = 0;
            if (*bytes_read == 0) return 0; // EOF
        }
        *buffer = byte_buffer[(*byte_pos)++];
        *bit_pos = 8;
    }
    // Extract the bit (LSB first to match write order)
    return (*buffer >> (--(*bit_pos))) & 1;
}



// Reads the header and builds the sequence lookup table
static BinarySequence* readHeader(FILE* file, uint16_t* sequence_count) {
    // Read sequence count (big-endian)
    uint8_t count_bytes[2];
    if (fread(count_bytes, 1, 2, file) != 2) {
        fprintf(stderr, "Error: Failed to read sequence count from header\n");
        return NULL;
    }
    *sequence_count = (count_bytes[0] << 8) | count_bytes[1];
    
    // Allocate sequence array
    BinarySequence* sequences = calloc(*sequence_count, sizeof(BinarySequence));
    if (!sequences) {
        fprintf(stderr, "Error: Memory allocation failed for sequences\n");
        return NULL;
    }
    
    // Initialize buffered reading
    uint8_t buffer[BUFFER_SIZE];
    size_t buf_pos = 0;
    size_t buf_size = 0;
    
    for (int i = 0; i < *sequence_count; i++) {
        // Refill buffer if needed
        if (buf_pos >= buf_size) {
            buf_size = fread(buffer, 1, BUFFER_SIZE, file);
            buf_pos = 0;
            if (buf_size == 0 && i != *sequence_count - 1) {
                fprintf(stderr, "Error: Unexpected EOF in header\n");
                free(sequences);
                return NULL;
            }
        }
        
        // Read header byte
        uint8_t header = buffer[buf_pos++];
        uint8_t length = header >> 5;
        uint8_t group = (header >> 3) & 0x03;
        uint8_t code_size = groupCodeSize(group); // Just codeword bits
        
        // Validate length
        if (length == 0 || length > SEQ_LENGTH_LIMIT) {
            fprintf(stderr, "Error: Invalid sequence length %d\n", length);
            free(sequences);
            return NULL;
        }
        
        // Calculate needed bytes (sequence + codeword)
        size_t needed_bytes = length + ((code_size + 7) / 8);
        
        // Handle buffer boundary
        if (buf_pos + needed_bytes > buf_size) {
            size_t remaining = buf_size - buf_pos;
            memmove(buffer, buffer + buf_pos, remaining);
            buf_size = remaining + fread(buffer + remaining, 1, BUFFER_SIZE - remaining, file);
            buf_pos = 0;
            
            if (buf_size < needed_bytes) {
                fprintf(stderr, "Error: Unexpected EOF in sequence data\n");
                free(sequences);
                return NULL;
            }
        }
        
        // Read sequence data
        BinarySequence* seq = &sequences[i];
        seq->length = length;
        seq->group = group;
        memcpy(seq->sequence, buffer + buf_pos, length);
        buf_pos += length;
        
        // Read codeword (MSB first)
        seq->codeword = 0;
        uint8_t codeword_bytes = (code_size + 7) / 8;
        for (int j = 0; j < codeword_bytes; j++) {
            seq->codeword = (seq->codeword << 8) | buffer[buf_pos++];
        }
        
        // Mask to codeword size
        if (code_size < 16) { // Prevent undefined behavior with <<
            seq->codeword &= (1 << code_size) - 1;
        }
        
        // Debug print (optional)
        #ifdef DEBUG
        printf("Loaded seq %d: group=%d, codeword=0x%X, len=%d, data=[", 
               i, group, seq->codeword, length);
        for (int k = 0; k < length; k++) printf("%02X ", seq->sequence[k]);
        printf("]\n");
        #endif
    }
    
    return sequences;
}



// Decompresses the data and writes binary output
static void decompressData(FILE* input, FILE* output, 
                         BinarySequence* sequences, uint16_t sequence_count) {
    uint8_t bit_buffer = 0;
    uint8_t bit_pos = 0;
    
    // Allocate aligned buffers
    uint8_t* byte_buffer = create_aligned_buffer();
    uint8_t* out_buffer = create_aligned_buffer();
    
    size_t byte_pos = 0;
    size_t bytes_read = 0;
    size_t out_pos = 0;

    while (1) {
        // Read flag bit (MSB first)
        uint8_t flag = read_bit(&bit_buffer, &bit_pos, input, byte_buffer, &byte_pos, &bytes_read);
        if (bytes_read == 0 && bit_pos == 0) break;

        if (flag == 0) {
            // Uncompressed byte - read next 8 bits MSB first
            uint8_t byte = 0;
            for (int i = 0; i < 8; i++) {
                uint8_t bit = read_bit(&bit_buffer, &bit_pos, input, byte_buffer, &byte_pos, &bytes_read);
                byte = (byte << 1) | bit;
            }
            
            // Write to output buffer
            out_buffer[out_pos++] = byte;
            if (out_pos == BUFFER_SIZE) {
                fwrite(out_buffer, 1, BUFFER_SIZE, output);
                out_pos = 0;
            }
        } else {
            // Compressed sequence - read group (2 bits MSB first)
            uint8_t group = 0;
            for (int i = 0; i < 2; i++) {
                uint8_t bit = read_bit(&bit_buffer, &bit_pos, input, byte_buffer, &byte_pos, &bytes_read);
                group = (group << 1) | bit;
            }
            
            // Read codeword (variable length, MSB first)
            uint8_t code_size = groupCodeSize(group); // Now returns just codeword bits
            uint16_t codeword = 0;
            for (int i = 0; i < code_size; i++) {
                uint8_t bit = read_bit(&bit_buffer, &bit_pos, input, byte_buffer, &byte_pos, &bytes_read);
                codeword = (codeword << 1) | bit;
            }
            
            // Lookup the sequence
            int found = 0;
            for (int i = 0; i < sequence_count; i++) {
                if (sequences[i].group == group && sequences[i].codeword == codeword) {
                    // Check buffer space
                    if (out_pos + sequences[i].length > BUFFER_SIZE) {
                        fwrite(out_buffer, 1, out_pos, output);
                        out_pos = 0;
                    }
                    // Write the sequence
                    memcpy(out_buffer + out_pos, sequences[i].sequence, sequences[i].length);
                    out_pos += sequences[i].length;
                    found = 1;
                    break;
                }
            }
            
            if (!found) {
                fprintf(stderr, "Error: No matching sequence for group %d, codeword 0x%X\n", 
                       group, codeword);
                // Handle error (maybe skip or abort)
            }
        }
    }

    // Flush remaining output
    if (out_pos > 0) {
        fwrite(out_buffer, 1, out_pos, output);
    }
    
    // Cleanup
    free(byte_buffer);
    free(out_buffer);
}


// Main decompression function for binary files
void decompressBinaryFile(const char* input_filename, const char* output_filename) {
    // Open files in binary mode
    FILE* input = fopen(input_filename, "rb");
    if (!input) {
        perror("Error opening input file");
        return;
    }
    FILE* output = fopen(output_filename, "wb");    
    if (!output) {
        perror("Error opening output file");
        fclose(input);
        return;
    }
    
    // Read header to get sequence dictionary
    uint16_t sequence_count;
    BinarySequence* sequences = readHeader(input, &sequence_count);
    if (!sequences) {
        fclose(input);
        fclose(output);
        return;
    }
    
    // Use aligned buffers for decompression
    decompressData(input, output, sequences, sequence_count);
    
    // Cleanup
    free(sequences);
    if (fclose(input) != 0) {
        perror("Warning: Error closing input file");
    }
    if (fclose(output) != 0) {
        perror("Warning: Error closing output file");
    }
}

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <compressed_path> <reconstructed_path>\n", argv[0]);
        fprintf(stderr, "Where:\n");
        fprintf(stderr, "  <compressed_path>    Path to compressed input file\n");
        fprintf(stderr, "  <reconstructed_path> Path for decompressed output file\n");
        return 1;
    }

    const char* compressed_path = argv[1];
    const char* reconstructed_path = argv[2];

    printf("Decompressing %s to %s...\n", compressed_path, reconstructed_path);
    decompressBinaryFile(compressed_path, reconstructed_path);
    printf("Decompression complete.\n");

    return 0;
}
