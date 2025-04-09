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
        case 0: return 4;
        case 1: return 4;
        case 2: return 4;
        case 3: return 12;
        default:
            fprintf(stderr, "Invalid group %d\n", group);
            return 0;
    }
}

#define BUFFER_SIZE (1024 * 1024)

static uint8_t findEndOfHeaderMarker(FILE* input, uint8_t* byte_buffer, size_t* byte_pos, size_t* bytes_read);
static uint16_t read_bits(uint8_t num_bits, uint8_t* bit_buffer, uint8_t* bit_pos,
                         uint8_t* byte_buffer, size_t* byte_pos, size_t* bytes_read, FILE* file);
                         
static void print_binary(uint8_t byte);
static BinarySequence* readHeader(FILE* file, uint16_t* sequence_count, uint8_t* byte_buffer, size_t* byte_pos, size_t* bytes_read);
                         
static uint8_t* create_aligned_buffer() {
    uint8_t* buf = aligned_alloc(64, BUFFER_SIZE);
    if (!buf) {
        fprintf(stderr, "Error: Failed to allocate aligned buffer\n");
        exit(EXIT_FAILURE);
    }
    return buf;
}

static BinarySequence* readHeader(FILE* file, uint16_t* sequence_count, uint8_t* byte_buffer, size_t* byte_pos, size_t* bytes_read) {
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
    *bytes_read = fread(byte_buffer, 1, BUFFER_SIZE, file);
    *byte_pos = 0;
    
    #ifdef DEBUG
    printf("Initial buffer load: %zu bytes\n", *bytes_read);
    if (*bytes_read > 0) {
        printf("First byte: 0x%02X\n", byte_buffer[*byte_pos]);
        printf("Binary: ");
        print_binary(byte_buffer[*byte_pos]);
        printf("\n");
    }
    #endif

    int i;
    for (i = 0; i < *sequence_count; i++) {
        #ifdef DEBUG
        printf("\nProcessing sequence %d/%d\n", i+1, *sequence_count);
        printf("Current position: byte %zu, bit %d\n", *byte_pos, bit_pos);
        #endif
        
        // 1. Read 3-bit length (MSB first)
        uint8_t length = read_bits(3, &bit_buffer, &bit_pos, byte_buffer, byte_pos, bytes_read, file);
        if (length == 0xFF) {
            fprintf(stderr, "Error: Failed to read length\n");
            goto error_cleanup;
        }
        sequences[i].length = length;
        
        #ifdef DEBUG
        printf("Read length: %d\n", length);
        #endif

        // 2. Read sequence bytes
        sequences[i].sequence = malloc(length);
        if (!sequences[i].sequence) {
            fprintf(stderr, "Error: Sequence data allocation failed\n");
            goto error_cleanup;
        }
        
        for (int j = 0; j < length; j++) {
            uint16_t byte_val = read_bits(8, &bit_buffer, &bit_pos, byte_buffer, byte_pos, bytes_read, file);
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
        uint8_t group = read_bits(2, &bit_buffer, &bit_pos, byte_buffer, byte_pos, bytes_read, file);
        if (group == 0xFF) {
            fprintf(stderr, "Error: Failed to read group\n");
            goto error_cleanup;
        }
        sequences[i].group = group;
        
        // 4. Read codeword (MSB first)
        uint8_t code_size = groupCodeSize(group);
        uint16_t codeword = read_bits(code_size, &bit_buffer, &bit_pos, byte_buffer, byte_pos, bytes_read, file);
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
    printf("Final position: byte %zu, bit %d\n", *byte_pos, bit_pos);
    printf("Remaining bytes: %zu\n", *bytes_read - *byte_pos);
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
uint16_t read_bits(uint8_t num_bits, uint8_t* bit_buffer, uint8_t* bit_pos,
                  uint8_t* byte_buffer, size_t* byte_pos, size_t* bytes_read, FILE* file) {
    uint16_t result = 0;
    
    for (uint8_t i = 0; i < num_bits; i++) {
        if (*bit_pos == 0) {
            if (*byte_pos >= *bytes_read) {
                *bytes_read = fread(byte_buffer, 1, BUFFER_SIZE, file);
                *byte_pos = 0;
                if (*bytes_read == 0) return 0xFFFF;
            }
            *bit_buffer = byte_buffer[(*byte_pos)++];
            *bit_pos = 8;
        }
        
        // CHANGED THIS LINE - shift before masking
        result = (result << 1) | ((*bit_buffer >> (*bit_pos - 1)) & 1);
        (*bit_pos)--;
    }
    
    return result;
}

// Helper function to print binary representation
static void print_binary(uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        printf("%d", (byte >> i) & 1);
    }
}

static uint8_t findEndOfHeaderMarker(FILE* input, uint8_t* byte_buffer, size_t* byte_pos, size_t* bytes_read) {
    #ifdef DEBUG
    printf("Searching for end-of-header marker (0xFF)\n");
    #endif
    
    while (1) {
        if (*byte_pos >= *bytes_read) {
            *bytes_read = fread(byte_buffer, 1, BUFFER_SIZE, input);
            *byte_pos = 0;
            if (*bytes_read == 0) {
                #ifdef DEBUG
                printf("Reached EOF without finding marker\n");
                #endif
                return 0;
            }
        }
        
        uint8_t current_byte = byte_buffer[*byte_pos];
        (*byte_pos)++;
        
        if (current_byte == 0xFF) {
            #ifdef DEBUG
            printf("Found end-of-header marker at byte %zu\n", *byte_pos - 1);
            #endif
            return 1;
        }
    }
}

static void decompressData(FILE* input, FILE* output, 
                         BinarySequence* sequences, uint16_t sequence_count,
                         uint8_t* byte_buffer, size_t* byte_pos, size_t* bytes_read) {
    #ifdef DEBUG
    printf("\n\n=== READING Data ===\n");
    #endif
    uint8_t* out_buffer = create_aligned_buffer();
    size_t out_pos = 0;
    uint8_t bit_buffer = 0;
    uint8_t bit_pos = 0;

    while (1) {
        // Read 1-bit flag
        uint16_t flag = read_bits(1, &bit_buffer, &bit_pos, byte_buffer, byte_pos, bytes_read, input);
        if (flag == 0xFFFF) {
            if (*bytes_read == 0) break; // Normal EOF
            fprintf(stderr, "Unexpected EOF\n");
            goto done;
        }

        if (flag == 0) {
            // Uncompressed data - read 8 bits (1 byte)
            uint16_t byte = read_bits(8, &bit_buffer, &bit_pos, byte_buffer, byte_pos, bytes_read, input);
            if (byte == 0xFFFF) {
                fprintf(stderr, "Unexpected EOF reading uncompressed byte\n");
                goto done;
            }

            #ifdef DEBUG
            printf("Read uncompressed byte: 0x%02X\n", byte);
            #endif

            out_buffer[out_pos++] = (uint8_t)byte;
            if (out_pos == BUFFER_SIZE) {
                fwrite(out_buffer, 1, BUFFER_SIZE, output);
                out_pos = 0;
            }
        } else {
            // Compressed data - read group and codeword
            uint16_t group = read_bits(2, &bit_buffer, &bit_pos, byte_buffer, byte_pos, bytes_read, input);
            if (group == 0xFFFF) {
                fprintf(stderr, "Unexpected EOF reading group\n");
                goto done;
            }

            uint8_t code_size = groupCodeSize((uint8_t)group);
			
            uint16_t codeword = read_bits(code_size, &bit_buffer, &bit_pos, byte_buffer, byte_pos, bytes_read, input);
            if (codeword == 0xFFFF) {
                fprintf(stderr, "Unexpected EOF reading codeword\n");
                goto done;
            }

            #ifdef DEBUG
            printf("Read compressed data - group: %d, codeword: 0x%04X (%d bits)\n", 
                  group, codeword, code_size);
            #endif

            // Find matching sequence
            int found = 0;
            for (int i = 0; i < sequence_count; i++) {
                if (sequences[i].group == group && sequences[i].codeword == codeword) {
                    #ifdef DEBUG
                    printf("Found matching sequence: ");
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
                fprintf(stderr, "Error: No match for group=%d codeword=0x%04X\n", group, codeword);
                goto done;
            }
        }

        #ifdef DEBUG
        //printf("Current output buffer (%zu bytes): ", out_pos);
        //for (size_t i = 0; i < out_pos; i++) {
          //  printf("%02X ", out_buffer[i]);
        //}
        printf("\n");
        #endif
    }

done:
    if (out_pos > 0) {
        fwrite(out_buffer, 1, out_pos, output);
    }
    free(out_buffer);
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

    uint8_t* byte_buffer = create_aligned_buffer();
    size_t byte_pos = 0;
    size_t bytes_read = 0;

    uint16_t sequence_count;
    BinarySequence* sequences = readHeader(input, &sequence_count, byte_buffer, &byte_pos, &bytes_read);
    if (!sequences) {
        fclose(input);
        fclose(output);
        free(byte_buffer);
        return;
    }
    
    #ifdef DEBUG
    printf("\nTransitioning to data section at offset %ld\n", ftell(input));
    printf("Initial data buffer contains %zu bytes\n", bytes_read);
    #endif

    // Find the end-of-header marker (0xFF)
    if (findEndOfHeaderMarker(input, byte_buffer, &byte_pos, &bytes_read)) {
        #ifdef DEBUG
        printf("Starting data decompression at byte %zu\n", byte_pos);
        #endif
        
        decompressData(input, output, sequences, sequence_count, byte_buffer, &byte_pos, &bytes_read);
    } else {
        fprintf(stderr, "Error: Could not find end-of-header marker\n");
    }

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
