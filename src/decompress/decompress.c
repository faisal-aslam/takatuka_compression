#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../constants.h"
#define DEBUG 1

typedef struct {
    uint8_t *sequence;
    int length;
    uint8_t group;
    uint8_t isUsed;
    uint16_t codeword;
} BinarySequence;

static inline uint8_t groupCodeSize(uint8_t group) {
    switch(group) {
        case 1: return 4;
        case 2: return 4;
        case 3: return 4;
        case 4: return 12;
        default:
            fprintf(stderr, "Invalid group %d\n", group);
            return 0;
    }
}

#define BUFFER_SIZE (1024 * 1024)

static uint16_t read_bits(uint8_t num_bits, uint8_t* bit_buffer, uint8_t* bit_pos,
                         uint8_t* byte_buffer, size_t* byte_pos, size_t* bytes_read, FILE* file);
                         
static void print_binary(uint8_t byte);
                         
static uint8_t* create_aligned_buffer() {
    uint8_t* buf = aligned_alloc(64, BUFFER_SIZE);
    if (!buf) {
        fprintf(stderr, "Error: Failed to allocate aligned buffer\n");
        exit(EXIT_FAILURE);
    }
    return buf;
}

static inline uint8_t read_bit(uint8_t* buffer, uint8_t* bit_pos,
                             FILE* file, uint8_t* byte_buffer,
                             size_t* byte_pos, size_t* bytes_read) {
    #ifdef DEBUG
    printf("[read_bit] Before: buffer=0x%02X, bit_pos=%d, byte_pos=%zu, bytes_read=%zu\n", 
           *buffer, *bit_pos, *byte_pos, *bytes_read);
    #endif
    
    if (*bit_pos == 8) {
        *bit_pos = 0;
        *buffer = 0;
    }
    
    if (*bit_pos == 0) {
        if (*byte_pos >= *bytes_read) {
            *bytes_read = fread(byte_buffer, 1, BUFFER_SIZE, file);
            *byte_pos = 0;
            if (*bytes_read == 0) {
                #ifdef DEBUG
                printf("[read_bit] Reached EOF\n");
                #endif
                return 0;
            }
            #ifdef DEBUG
            printf("[read_bit] Refilled buffer with %zu bytes\n", *bytes_read);
            #endif
        }
        *buffer = byte_buffer[(*byte_pos)++];
        #ifdef DEBUG
        printf("[read_bit] Loaded new byte: 0x%02X\n", *buffer);
        #endif
    }

    uint8_t bit = (*buffer >> (7 - *bit_pos)) & 1;
    (*bit_pos)++;
    
    #ifdef DEBUG
    printf("[read_bit] Read bit %d from byte 0x%02X: %d\n", *bit_pos-1, *buffer, bit);
    #endif
    
    return bit;
}


static BinarySequence* readHeader(FILE* file, uint16_t* sequence_count) {
    #ifdef DEBUG
    printf("\n=== READING HEADER ===\n");
    #endif
    
    // Read sequence count (2 bytes big-endian)
    uint8_t count_bytes[2];
    if (fread(count_bytes, 1, 2, file) != 2) {
        fprintf(stderr, "Error: Failed to read sequence count\n");
        return NULL;
    }
    *sequence_count = (count_bytes[0] << 8) | count_bytes[1];
    
    #ifdef DEBUG
    printf("Header indicates %d sequences\n", *sequence_count);
    printf("Count bytes: %02X %02X\n", count_bytes[0], count_bytes[1]);
    #endif
    
    BinarySequence* sequences = calloc(*sequence_count, sizeof(BinarySequence));
    if (!sequences) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return NULL;
    }
    
    uint8_t bit_buffer = 0;
    uint8_t bit_pos = 0;
    uint8_t byte_buffer[BUFFER_SIZE];
    size_t byte_pos = 0;
    size_t bytes_read = fread(byte_buffer, 1, BUFFER_SIZE, file);
    
    #ifdef DEBUG
    printf("Initial buffer load: %zu bytes\n", bytes_read);
    if (bytes_read > 0) {
        printf("First byte: 0x%02X\n", byte_buffer[0]);
        printf("Binary: ");
        print_binary(byte_buffer[0]);
        printf("\n");
    }
    #endif

    int i;
    for (i = 0; i < *sequence_count; i++) {
        #ifdef DEBUG
        printf("\nProcessing sequence %d/%d\n", i+1, *sequence_count);
        printf("Current position: byte %zu, bit %d\n", byte_pos, bit_pos);
        #endif
        
        // 1. Read 3-bit length (MSB first)
        uint8_t length = read_bits(3, &bit_buffer, &bit_pos, byte_buffer, &byte_pos, &bytes_read, file);
        if (length == 0xFF) {
            fprintf(stderr, "Error: Failed to read length\n");
            goto error_cleanup;
        }
        sequences[i].length = length;
        
        #ifdef DEBUG
        printf("Read length: %d\n", length);
        #endif

        // 2. Read sequence bytes (handles byte splitting)
        sequences[i].sequence = malloc(length);
        if (!sequences[i].sequence) {
            fprintf(stderr, "Error: Sequence data allocation failed\n");
            goto error_cleanup;
        }
        
        for (int j = 0; j < length; j++) {
			int byte_val = read_bits(8, &bit_buffer, &bit_pos, byte_buffer, &byte_pos, &bytes_read, file);
			if (byte_val == 0xFFFF) {
				fprintf(stderr, "Error: Failed to read sequence byte\n");
				goto error_cleanup;
			}
			sequences[i].sequence[j] = (uint8_t)byte_val;
		}

        #ifdef DEBUG
        printf("Read sequence data (%d bytes): ", length);
        for (int j = 0; j < length; j++) printf("%02X ", sequences[i].sequence[j]);
        printf("\n");
        #endif

        // 3. Read 2-bit group (MSB first)
        uint8_t group = read_bits(2, &bit_buffer, &bit_pos, byte_buffer, &byte_pos, &bytes_read, file);
        if (group == 0xFF) {
            fprintf(stderr, "Error: Failed to read group\n");
            goto error_cleanup;
        }
        sequences[i].group = group;
        
        // 4. Read codeword (MSB first)
        uint8_t code_size = groupCodeSize(group);
        uint16_t codeword = read_bits(code_size, &bit_buffer, &bit_pos, byte_buffer, &byte_pos, &bytes_read, file);
        if (codeword == 0xFFFF) {
            fprintf(stderr, "Error: Failed to read codeword\n");
            goto error_cleanup;
        }
        sequences[i].codeword = codeword;
        
        #ifdef DEBUG
        printf("Read group: %d, codeword: %d (%d bits)\n", group, codeword, code_size);
        #endif
    }
    
    #ifdef DEBUG
    printf("\n=== HEADER READ COMPLETE ===\n");
    printf("Final position: byte %zu, bit %d\n", byte_pos, bit_pos);
    printf("Remaining bytes: %zu\n", bytes_read - byte_pos);
    #endif
    
    return sequences;

error_cleanup:
    for (int j = 0; j < i; j++) {
        if (sequences[j].sequence) free(sequences[j].sequence);
    }
    free(sequences);
    return NULL;
}

/**
 * @brief Reads bits from buffer (MSB first)
 */
static uint16_t read_bits(uint8_t num_bits, uint8_t* bit_buffer, uint8_t* bit_pos,
                         uint8_t* byte_buffer, size_t* byte_pos, size_t* bytes_read, FILE* file) {
    uint16_t result = 0;
    
    #ifdef DEBUG
    printf("Reading %d bits: ", num_bits);
    #endif
    
    for (uint8_t i = 0; i < num_bits; i++) {
        if (*bit_pos == 0) {
            if (*byte_pos >= *bytes_read) {
                *bytes_read = fread(byte_buffer, 1, BUFFER_SIZE, file);
                *byte_pos = 0;
                if (*bytes_read == 0) {
                    #ifdef DEBUG
                    printf("\nError: Unexpected EOF\n");
                    #endif
                    return 0xFFFF; // Error
                }
            }
            *bit_buffer = byte_buffer[(*byte_pos)++];
            *bit_pos = 8;
        }
        
        result = (result << 1) | ((*bit_buffer >> 7) & 1);
        *bit_buffer <<= 1;
        (*bit_pos)--;
        
        #ifdef DEBUG
        printf("%d", (result & 1));
        #endif
    }
    
    #ifdef DEBUG
    printf(" -> 0x%04X\n", result);
    #endif
    
    return result;
}

// Helper function to print binary representation
static void print_binary(uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        printf("%d", (byte >> i) & 1);
    }
}



static void decompressData(FILE* input, FILE* output, 
                         BinarySequence* sequences, uint16_t sequence_count,
                         uint8_t* byte_buffer, size_t bytes_read) {
    uint8_t bit_buffer = 0;
    uint8_t bit_pos = 0;
    uint8_t* out_buffer = create_aligned_buffer();
    size_t byte_pos = 0;
    size_t out_pos = 0;

    #ifdef DEBUG
    printf("\n=== STARTING DATA DECOMPRESSION ===\n");
    printf("Initial state: bit_buffer=0x%02X, bit_pos=%d\n", bit_buffer, bit_pos);
    printf("Initial data buffer: %zu bytes\n", bytes_read);
    #endif

    while (1) {
        #ifdef DEBUG
        printf("\n--- Processing new element ---\n");
        printf("Current buffer: byte_pos=%zu, bytes_read=%zu\n", byte_pos, bytes_read);
        #endif

        uint8_t flag = read_bit(&bit_buffer, &bit_pos, input, byte_buffer, &byte_pos, &bytes_read);
        #ifdef DEBUG
        printf("Read flag bit: %d\n", flag);
        #endif

        if (bytes_read == 0 && bit_pos == 0) {
            #ifdef DEBUG
            printf("Reached end of input data\n");
            #endif
            break;
        }

        if (flag == 0) {
            #ifdef DEBUG
            printf("Processing UNCOMPRESSED byte\n");
            #endif
            
            uint8_t byte = 0;
            for (int i = 0; i < 8; i++) {
                uint8_t bit = read_bit(&bit_buffer, &bit_pos, input, byte_buffer, &byte_pos, &bytes_read);
                byte = (byte << 1) | bit;
                #ifdef DEBUG
                printf("Read bit %d: %d (byte so far: 0x%02X)\n", i, bit, byte);
                #endif
            }
            
            #ifdef DEBUG
            printf("Complete uncompressed byte: 0x%02X\n", byte);
            #endif
            
            out_buffer[out_pos++] = byte;
            if (out_pos == BUFFER_SIZE) {
                fwrite(out_buffer, 1, BUFFER_SIZE, output);
                out_pos = 0;
            }
        } else {
            #ifdef DEBUG
            printf("Processing COMPRESSED sequence\n");
            #endif
            
            uint8_t group = 0;
            for (int i = 0; i < 2; i++) {
                uint8_t bit = read_bit(&bit_buffer, &bit_pos, input, byte_buffer, &byte_pos, &bytes_read);
                group = (group << 1) | bit;
                #ifdef DEBUG
                printf("Read group bit %d: %d (group so far: %d)\n", i, bit, group);
                #endif
            }
            
            uint8_t codeword_bits = groupCodeSize(group);
            uint16_t codeword = 0;
            #ifdef DEBUG
            printf("Reading %d codeword bits\n", codeword_bits);
            #endif
            
            for (int i = 0; i < codeword_bits; i++) {
                uint8_t bit = read_bit(&bit_buffer, &bit_pos, input, byte_buffer, &byte_pos, &bytes_read);
                codeword = (codeword << 1) | bit;
                #ifdef DEBUG
                printf("Read codeword bit %d: %d (codeword so far: 0x%04X)\n", i, bit, codeword);
                #endif
            }
            
            #ifdef DEBUG
            printf("Complete pattern: group=%d, codeword=0x%04X\n", group, codeword);
            printf("Searching %d sequences for match...\n", sequence_count);
            #endif
            
            int found = 0;
            for (int i = 0; i < sequence_count; i++) {
                if (sequences[i].group == group && sequences[i].codeword == codeword) {
                    #ifdef DEBUG
                    printf("MATCH FOUND at index %d\n", i);
                    printf("Sequence length: %d\n", sequences[i].length);
                    printf("Sequence data: ");
                    for (int j = 0; j < sequences[i].length; j++) {
                        printf("%02X ", sequences[i].sequence[j]);
                    }
                    printf("\n");
                    #endif
                    
                    if (out_pos + sequences[i].length > BUFFER_SIZE) {
                        fwrite(out_buffer, 1, out_pos, output);
                        out_pos = 0;
                    }
                    
                    memcpy(out_buffer + out_pos, sequences[i].sequence, sequences[i].length);
                    out_pos += sequences[i].length;
                    found = 1;
                    break;
                }
            }
            
            if (!found) {
                fprintf(stderr, "Error: No matching sequence for group %d codeword 0x%04X\n", 
                       group, codeword);
                #ifdef DEBUG
                printf("Available sequences:\n");
                for (int i = 0; i < sequence_count; i++) {
                    printf("  Seq %d: group=%d, codeword=0x%04X, len=%d [", 
                           i, sequences[i].group, sequences[i].codeword, sequences[i].length);
                    for (int j = 0; j < sequences[i].length; j++) {
                        printf("%02X ", sequences[i].sequence[j]);
                    }
                    printf("]\n");
                }
                #endif
            }
        }
    }

    if (out_pos > 0) {
        fwrite(out_buffer, 1, out_pos, output);
    }
    
    free(out_buffer);
    
    #ifdef DEBUG
    printf("\n=== DECOMPRESSION COMPLETE ===\n");
    #endif
}

void decompressBinaryFile(const char* input_filename, const char* output_filename) {
    FILE* input = fopen(input_filename, "rb");
    FILE* output = fopen(output_filename, "wb");
    if (!input || !output) {
        perror("Error opening files");
        if (input) fclose(input);
        if (output) fclose(output);
        return;
    }

    uint16_t sequence_count;
    BinarySequence* sequences = readHeader(input, &sequence_count);
    if (!sequences) {
        fclose(input);
        fclose(output);
        return;
    }

    uint8_t* byte_buffer = create_aligned_buffer();
    size_t bytes_read = fread(byte_buffer, 1, BUFFER_SIZE, input);
    
    #ifdef DEBUG
    printf("\nTransitioning to data section at offset %ld\n", ftell(input));
    printf("Initial data buffer contains %zu bytes\n", bytes_read);
    #endif

    decompressData(input, output, sequences, sequence_count, byte_buffer, bytes_read);

    for (int i = 0; i < sequence_count; i++) {
        free(sequences[i].sequence);
    }
    free(sequences);
    free(byte_buffer);
    fclose(input);
    fclose(output);
}

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input> <output>\n", argv[0]);
        return 1;
    }

    printf("Decompressing %s to %s...\n", argv[1], argv[2]);
    decompressBinaryFile(argv[1], argv[2]);
    printf("Done.\n");

    return 0;
}
