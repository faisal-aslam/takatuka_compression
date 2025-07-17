// Combined C and H Files

// === FILE: /home/noman/takatuka/takatuka_compression/src/files/decompression/bit_reader.c ===

// bit_reader.c
#include "bit_reader.h"
#include <stdlib.h>

void bitreader_init(BitReader* br, const uint8_t* buffer, size_t size) {
    br->buffer = buffer;
    br->buffer_size = size;
    br->byte_pos = 0;
    br->bit_pos = 0;
    br->overflow = false;
}

bool bitreader_read(BitReader* br, uint32_t* value, uint8_t num_bits) {
    if (num_bits > 32 || br->overflow) return false;
    *value = 0;

    for (int i = num_bits - 1; i >= 0; --i) {
        if (br->byte_pos >= br->buffer_size) {
            br->overflow = true;
            return false;
        }

        uint8_t current_byte = br->buffer[br->byte_pos];
        uint8_t bit = (current_byte >> (7 - br->bit_pos)) & 1;
        *value |= (bit << i);

        br->bit_pos++;
        if (br->bit_pos == 8) {
            br->bit_pos = 0;
            br->byte_pos++;
        }
    }
    return true;
}

uint8_t* bitreader_load_from_file(FILE* fp, size_t* out_size) {
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    rewind(fp);

    uint8_t* buffer = malloc(size);
    if (!buffer) return NULL;

    if (fread(buffer, 1, size, fp) != size) {
        free(buffer);
        return NULL;
    }

    if (out_size) *out_size = size;
    return buffer;
}

void bitreader_print_state(const BitReader* br) {
    printf("[BitReader] byte_pos = %zu, bit_pos = %u, total_bits = %zu, overflow = %s\n",
           br->byte_pos, br->bit_pos,
           br->byte_pos * 8 + br->bit_pos,
           br->overflow ? "true" : "false");
}


void bitreader_reset(BitReader* br, const uint8_t* new_buffer, size_t new_size) {
    br->buffer = new_buffer;
    br->buffer_size = new_size;
    br->byte_pos = 0;
    br->bit_pos = 0;
    br->overflow = false;
}


void bitreader_attach_file(BitReader* br, FILE* file, size_t buffer_cap) {
    br->owned_buf = malloc(buffer_cap);
    if (!br->owned_buf) {
        fprintf(stderr, "Failed to allocate internal bitreader buffer\n");
        exit(EXIT_FAILURE);
    }

    br->file = file;
    br->buffer_cap = buffer_cap;

    size_t bytes_read = fread(br->owned_buf, 1, buffer_cap, br->file);
    br->buffer = br->owned_buf;
    br->buffer_size = bytes_read;
    br->byte_pos = 0;
    br->bit_pos = 0;
    br->overflow = false;
}

bool bitreader_fill_next_chunk(BitReader* br) {
    if (!br->file || !br->owned_buf) return false;

    size_t bytes_read = fread(br->owned_buf, 1, br->buffer_cap, br->file);
    if (bytes_read == 0) {
        br->overflow = true;
        return false;
    }

    br->buffer = br->owned_buf;
    br->buffer_size = bytes_read;
    br->byte_pos = 0;
    br->bit_pos = 0;
    br->overflow = false;

    return true;
}

// === FILE: /home/noman/takatuka/takatuka_compression/src/files/decompression/bit_reader.h ===

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct {
    const uint8_t* buffer;
    size_t buffer_size;
    size_t byte_pos;
    uint8_t bit_pos;  // 0 (MSB) to 7 (LSB)
    bool overflow;

    FILE* file;           // Optional: file to refill from
    uint8_t* owned_buf;   // Internal buffer owned by reader
    size_t   buffer_cap;  // Capacity of owned_buf
} BitReader;

void bitreader_attach_file(BitReader* br, FILE* file, size_t buffer_cap);
bool bitreader_fill_next_chunk(BitReader* br);

void bitreader_init(BitReader* br, const uint8_t* buffer, size_t size);
bool bitreader_read(BitReader* br, uint32_t* value, uint8_t num_bits);
uint8_t* bitreader_load_from_file(FILE* fp, size_t* out_size);

void bitreader_print_state(const BitReader* br); 
void bitreader_reset(BitReader* br, const uint8_t* new_buffer, size_t new_size);

// === FILE: /home/noman/takatuka/takatuka_compression/src/files/decompression/decompress_body.c ===

//decompress_body.c 
#include "decompress_body.h"

void read_body_using_code_map(FILE* file_to_read) {
    //todo we will writ this function later on.
}
// === FILE: /home/noman/takatuka/takatuka_compression/src/files/decompression/decompress_body.h ===

//decompress_header.h

#pragma once

#include <stdint.h>
#include <stdio.h>
#include "code_map.h"

// Read the body using the code map.
void read_body_using_code_map(FILE* file_to_read);

// === FILE: /home/noman/takatuka/takatuka_compression/src/files/decompression/decompress.c ===

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h> 
#include "decompress.h"
#include "code_classes.h"
#include "code_map.h"
#include "decompress_header.h"
#include "decompress_body.h"

void read_compressed_file(const char* filename, const uint8_t* block) {


    if (!filename || !block) {
        fprintf(stderr, "Error: Invalid inputs in writeCompressedOutput\n");
        return;
    }

    FILE *file = fopen(filename, "rb");

    //We first read header of compress file and create code_map.
    read_header_and_create_code_map(file);

    //Now using that code map we decompress the body of compressed file.
    read_body_using_code_map(file);

    if (!file) {
        perror("Failed to open binary reading file");
        return;
    } 

}
// === FILE: /home/noman/takatuka/takatuka_compression/src/files/decompression/decompress.h ===

#pragma once
// Rest of header content

#include "best_path_view.h"

void read_compressed_file(const char* filename, const uint8_t* block) ;

// === FILE: /home/noman/takatuka/takatuka_compression/src/files/decompression/decompress_header.c ===

#include "decompress_header.h"
#include "bit_reader.h"
#include "../map/code_map.h"
#include "code_classes.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define HEADER_BUFFER_SIZE 4096

CodeMap code_map_read;

// Macro to safely read bits, refilling buffer from file if needed
#define SAFE_BITREAD(reader_ptr, out, bits)                              \
    do {                                                                 \
        if (!bitreader_read(reader_ptr, out, bits)) {                    \
            if (!bitreader_fill_next_chunk(reader_ptr)) {                \
                fprintf(stderr, "Failed to read %u bits (EOF/overflow)\n", bits); \
                exit(EXIT_FAILURE);                                      \
            }                                                            \
            if (!bitreader_read(reader_ptr, out, bits)) {                \
                fprintf(stderr, "bitreader_read failed again\n");        \
                exit(EXIT_FAILURE);                                      \
            }                                                            \
        }                                                                \
    } while (0)

    
void read_header_and_create_code_map(FILE* file_to_read) {
    BitReader reader;
    bitreader_attach_file(&reader, file_to_read, HEADER_BUFFER_SIZE);

    uint32_t num_codes;
    SAFE_BITREAD(&reader, &num_codes, 16);
#ifdef DEBUG
    printf("[DEBUG] ⏎ Read 16 bits: num_codes = %u\n", num_codes);
    bitreader_print_state(&reader);
#endif

    init_code_map(&code_map_read, num_codes);

    for (uint32_t i = 0; i < num_codes; ++i) {
        uint32_t code_class, length, code_index;

        SAFE_BITREAD(&reader, &code_class, 2);
#ifdef DEBUG
        printf("[DEBUG] ⏎ Read 2 bits: code_class = %u\n", code_class);
        bitreader_print_state(&reader);
#endif

        SAFE_BITREAD(&reader, &length, 8);
#ifdef DEBUG
        printf("[DEBUG] ⏎ Read 8 bits: sequence_length = %u\n", length);
        bitreader_print_state(&reader);
#endif

        uint8_t class_bits = get_code_class_size((uint8_t)code_class);
        SAFE_BITREAD(&reader, &code_index, class_bits);
#ifdef DEBUG
        printf("[DEBUG] ⏎ Read %u bits: code_index = %u (class %u)\n", class_bits, code_index, code_class);
        bitreader_print_state(&reader);
#endif

        uint8_t* sequence = malloc(length);
        if (!sequence) {
            fprintf(stderr, "Memory allocation failure for sequence\n");
            exit(EXIT_FAILURE);
        }

        for (uint8_t j = 0; j < length; ++j) {
            uint32_t byte_val;
            SAFE_BITREAD(&reader, &byte_val, 8);
            sequence[j] = (uint8_t)byte_val;
#ifdef DEBUG
            printf("[DEBUG] ⏎ Read byte #%u: %02X\n", j, sequence[j]);
            bitreader_print_state(&reader);
#endif
        }

        code_map_set(&code_map_read, sequence, (uint8_t)length, code_index, (uint8_t)code_class);
        // Do not free sequence — it's owned by CodeMap
    }

#ifdef DEBUG
    printf("[DEBUG] Completed CodeMap reconstruction.\n");
#endif

    free(reader.owned_buf);  // Clean up internal buffer
}

// === FILE: /home/noman/takatuka/takatuka_compression/src/files/decompression/decompress_header.h ===

//decompress_header.h

#pragma once

#include <stdint.h>
#include <stdio.h>
#include "code_map.h"

// Reconstructs CodeMap by reading the header of the compressed file
void read_header_and_create_code_map(FILE* file_to_read);

