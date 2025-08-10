// Combined C and H Files

// === FILE: /home/noman/takatuka/takatuka_compression/src/files/code_classes.h ===

//code_class.h

#pragma once

#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define TOTAL_NUMBER_OF_CODE_CLASSES 4

uint8_t get_code_class_overhead(uint8_t code_class);

/* NOTE: add class2_bits - used when code_class == 2 to compute correct size */
uint8_t get_header_overhead(uint8_t code_class, uint16_t seq_length, uint8_t class2_bits);

uint16_t get_code_class_threshold(uint8_t code_class, uint8_t class2_bits);
uint8_t get_code_class_size(uint8_t code_class, uint8_t class2_bits);

uint8_t calculate_class2_bits(uint16_t class2_codes);

// === FILE: /home/noman/takatuka/takatuka_compression/src/files/code_classes.c ===

//code_class.c

// src/files/code_classes.c

#include "code_classes.h"

/*
 * Header overhead layout per sequence entry (in bits):
 *  - 3 bits : (reserved for something in your original format — keep it)
 *  - N bytes * 8 : sequence bytes
 *  - 2 bits : code_class prefix
 *  - M bits : code index (depends on class: class0/class1 fixed, class2 dynamic)
 *
 * get_header_overhead returns this total in BITS or BYTES? Your old code
 * returned an integer that was used only for relative calculations; keep the same
 * semantics (here returning number of bits).
 */
uint8_t get_header_overhead(uint8_t code_class, uint16_t seq_length, uint8_t class2_bits) {
    if (code_class == 0 || code_class == 1 || code_class == 2) {
        /* 3 bits + seq_length*8 + 2 bits class + code bits for that class */
        uint8_t code_bits = get_code_class_size(code_class, class2_bits);
        return (uint8_t)(3 + (seq_length * 8) + 2 + code_bits);
    } else {
        fprintf(stderr, "Invalid code_class %d Exiting (get_header_overhead)!\n", code_class);
        exit(EXIT_FAILURE);
        return 0;
    }
}

/*
 * calculate_class2_bits:
 *  - returns smallest n such that (1u << n) >= class2_codes
 *  - returns 0 for class2_codes == 0
 *  - returns 0 for class2_codes == 1 (i.e. 1 entry requires 0 index bits)
 *    If you prefer a minimum of 1 bit for any non-zero count, change the return
 *    to `return (n == 0) ? 1 : n;`.
 */
uint8_t calculate_class2_bits(uint16_t class2_codes) {
    if (class2_codes == 0) return 0;

    uint8_t n = 0;
    while ((n < 31) && ((1u << n) < (uint32_t)class2_codes)) {
        n++;
    }
    return n;
}

/* Return the number of bits used for the index portion (excluding the 2-bit class prefix) */
uint8_t get_code_class_size(uint8_t code_class, uint8_t class2_bits) {
    switch (code_class) {
        case 0: return 4;              /* fixed */
        case 1: return 5;              /* fixed — note you had 5 in your last edit */
        case 2: return class2_bits;    /* dynamic */
        default:
            fprintf(stderr, "Invalid code_class %d Exiting (get_code_class_size)!\n", code_class);
            exit(EXIT_FAILURE);
            return 0;
    }
}

/* Return threshold (capacity) for given class as uint16_t. For class2 uses class2_bits. */
uint16_t get_code_class_threshold(uint8_t code_class, uint8_t class2_bits) {
    uint8_t bits;
    if (code_class == 2) {
        bits = class2_bits;
    } else {
        bits = get_code_class_size(code_class, 0); /* class2_bits unused for non-class2 */
    }

    if (bits >= 16) {
        /* uint16_t return can't represent >2^15 safely here. If you expect >65535
           entries for a class2, change return type to uint32_t. */
        fprintf(stderr, "Requested threshold bits too large: %u\n", bits);
        exit(EXIT_FAILURE);
    }
    return (uint16_t)(1u << bits);
}

/* Returns overhead of a code_class (kept same as before) */
uint8_t get_code_class_overhead(uint8_t code_class) {
    (void)code_class; // unused for now
    return 3;
}

// === FILE: /home/noman/takatuka/takatuka_compression/src/files/decompression//bit_reader.c ===

// bit_reader.c
#include "bit_reader.h"
#include <stdlib.h>

void bitreader_init(BitReader *br, const uint8_t *buffer, size_t size) {
    br->buffer = buffer;
    br->buffer_size = size;
    br->byte_pos = 0;
    br->bit_pos = 0;
    br->overflow = false;
}

bool bitreader_read(BitReader *br, uint32_t *value, uint8_t num_bits) {
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

void bitreader_move_byte_boundary(BitReader *br) {
    if (br->bit_pos != 0) {
        br->byte_pos++;
        br->bit_pos = 0;
    }
}

uint8_t *bitreader_load_from_file(FILE *fp, size_t *out_size) {
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    rewind(fp);

    uint8_t *buffer = malloc(size);
    if (!buffer) return NULL;

    if (fread(buffer, 1, size, fp) != size) {
        free(buffer);
        return NULL;
    }

    if (out_size) *out_size = size;
    return buffer;
}

void bitreader_print_state(const BitReader *br) {
    printf("[BitReader] byte_pos = %zu, bit_pos = %u, total_bits = %zu, overflow = %s\n", br->byte_pos, br->bit_pos,
           br->byte_pos * 8 + br->bit_pos, br->overflow ? "true" : "false");
}

void bitreader_reset(BitReader *br, const uint8_t *new_buffer, size_t new_size) {
    br->buffer = new_buffer;
    br->buffer_size = new_size;
    br->byte_pos = 0;
    br->bit_pos = 0;
    br->overflow = false;
}

void bitreader_attach_file(BitReader *br, FILE *file, size_t buffer_cap) {
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

bool bitreader_fill_next_chunk(BitReader *br) {
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

bool bitreader_peek(BitReader *br, uint32_t *value, uint8_t num_bits) {
    size_t saved_byte_pos = br->byte_pos;
    uint8_t saved_bit_pos = br->bit_pos;
    bool saved_overflow = br->overflow;

    bool success = bitreader_read(br, value, num_bits);

    // Restore state
    br->byte_pos = saved_byte_pos;
    br->bit_pos = saved_bit_pos;
    br->overflow = saved_overflow;

    return success;
}

void bitreader_close(BitReader *br) {
    if (br->owned_buf) {
        free(br->owned_buf);
        br->owned_buf = NULL;
    }

    br->buffer = NULL;
    br->buffer_size = 0;
    br->byte_pos = 0;
    br->bit_pos = 0;
    br->overflow = false;
    br->file = NULL;
    br->buffer_cap = 0;
}

// === FILE: /home/noman/takatuka/takatuka_compression/src/files/decompression//bit_reader.h ===

//bit_reader.h

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

bool bitreader_peek(BitReader* br, uint32_t* value, uint8_t num_bits);

void bitreader_close(BitReader* br);

void bitreader_move_byte_boundary(BitReader *br);

// === FILE: /home/noman/takatuka/takatuka_compression/src/files/decompression//decoder_map.c ===

#include "decoder_map.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void init_decoder_map(DecoderMap* map, size_t capacity) {
    map->capacity = capacity * 2; // Maintain load factor ≤ 0.5
    map->size = 0;
    map->entries = calloc(map->capacity, sizeof(DecoderMapEntry));
}

void free_decoder_map(DecoderMap* map) {
    free(map->entries);
    map->entries = NULL;
    map->capacity = 0;
    map->size = 0;
}

static inline size_t decoder_hash(uint16_t code, uint8_t code_class) {
    return ((uint32_t)code << 3) | (code_class & 0x07); // simple mix
}

static inline size_t decoder_probe(size_t hash, size_t i, size_t cap) {
    return (hash + i) % cap;
}

bool decoder_map_set(DecoderMap* map, uint16_t code, uint8_t code_class, const uint8_t* seq, uint8_t length) {
    if (map->size >= map->capacity / 2) {
        return false; // load factor too high
    }

    size_t h = decoder_hash(code, code_class);
    for (size_t i = 0; i < map->capacity; ++i) {
        size_t idx = decoder_probe(h, i, map->capacity);
        DecoderMapEntry* e = &map->entries[idx];

        if (!e->occupied) {
            e->code = code;
            e->code_class = code_class;
            e->seq = seq;
            e->length = length;
            e->occupied = true;
            map->size++;
            return true;
        }

        if (e->code == code && e->code_class == code_class) {
            e->seq = seq;
            e->length = length;
            return true; // update existing
        }
    }
    return false;
}

bool decoder_map_get(const DecoderMap* map, uint16_t code, uint8_t code_class, const uint8_t** out_seq, uint8_t* out_len) {
    size_t h = decoder_hash(code, code_class);
    for (size_t i = 0; i < map->capacity; ++i) {
        size_t idx = decoder_probe(h, i, map->capacity);
        const DecoderMapEntry* e = &map->entries[idx];

        if (!e->occupied) return false;
        if (e->code == code && e->code_class == code_class) {
            *out_seq = e->seq;
            *out_len = e->length;
            return true;
        }
    }
    return false;
}

void print_decoder_map(const DecoderMap* map) {
    printf("DecoderMap (size: %zu, capacity: %zu):\n", map->size, map->capacity);
    printf("---------------------------------------------------\n");
    printf("| Index | Code | Class | Length | Sequence\n");
    printf("---------------------------------------------------\n");

    for (size_t i = 0; i < map->capacity; ++i) {
        const DecoderMapEntry* e = &map->entries[i];
        if (!e->occupied) continue;

        printf("| %5zu | %4u | %5u | %6u | ", i, e->code, e->code_class, e->length);
        for (uint8_t j = 0; j < e->length; ++j) {
            printf("%02X ", e->seq[j]);
        }
        printf("\n");
    }
    printf("---------------------------------------------------\n");
}

// === FILE: /home/noman/takatuka/takatuka_compression/src/files/decompression//decoder_map.h ===

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    const uint8_t* seq;
    uint8_t length;
    uint16_t code;
    uint8_t code_class;
    bool occupied;
} DecoderMapEntry;

typedef struct {
    DecoderMapEntry* entries;
    size_t capacity;
    size_t size;
} DecoderMap;

void init_decoder_map(DecoderMap* map, size_t capacity);
void free_decoder_map(DecoderMap* map);

bool decoder_map_set(DecoderMap* map, uint16_t code, uint8_t code_class, const uint8_t* seq, uint8_t length);
bool decoder_map_get(const DecoderMap* map, uint16_t code, uint8_t code_class, const uint8_t** out_seq, uint8_t* out_len);

void print_decoder_map(const DecoderMap* map);

// === FILE: /home/noman/takatuka/takatuka_compression/src/files/decompression//decompress_body.c ===

#include "decompress_body.h"
#include "bit_reader.h"
#include "code_classes.h"
#include "decoder_map.h"
#include "decompress_header.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define BODY_BUFFER_SIZE 4096

void read_body_using_decoder_map(BitReader *reader, const char *decompress_file_name) {
    FILE *output_file = fopen(decompress_file_name, "wb");
    if (!output_file) {
        fprintf(stderr, "Failed to open output file: %s\n", decompress_file_name);
        exit(EXIT_FAILURE);
    }

    uint8_t output[256]; // Max sequence size
    uint32_t bit;
    size_t total_bytes_written = 0;

#ifdef DEBUG
    printf("\n=== STARTING BODY DECOMPRESSION ===\n");
    printf("Output file: %s\n", decompress_file_name);
    bitreader_print_state(reader);
#endif

    while (bitreader_read(reader, &bit, 1)) {
#ifdef DEBUG
        printf("\n[READ] Prefix bit: %u\n", bit);
        bitreader_print_state(reader);
#endif

        if (bit == 0) {
            // Uncompressed single byte
            uint32_t byte;
            if (!bitreader_read(reader, &byte, 8)) {
                if (reader->bit_pos == 0 && reader->byte_pos >= reader->buffer_size) {
                    // Graceful EOF: don't throw error
                    break;
                }
                fprintf(stderr, "Unexpected EOF while reading uncompressed byte\n");
                exit(EXIT_FAILURE);
            }

#ifdef DEBUG
            printf("[UNCOMPRESSED] Byte: 0x%02X (%c)\n", byte, (byte >= 32 && byte <= 126) ? byte : '.');
            printf("  Writing 1 raw byte to output\n");
#endif

            fputc((uint8_t)byte, output_file);
            total_bytes_written++;
        } else {
            // Compressed data - read code class to determine type
            uint32_t code_class;
            if (!bitreader_read(reader, &code_class, 2)) {
                fprintf(stderr, "Failed to read code_class\n");
                exit(EXIT_FAILURE);
            }

#ifdef DEBUG
            printf("[COMPRESSED] Read code class: %u\n", code_class);
            bitreader_print_state(reader);
#endif

            if (code_class == 3) { // 0b11 indicates RLE
                // RLE case
                uint32_t rle_len;
                if (!bitreader_read(reader, &rle_len, 3)) {
                    fprintf(stderr, "Failed to read RLE length\n");
                    exit(EXIT_FAILURE);
                }

                uint32_t rle_count;
                if (!bitreader_read(reader, &rle_count, 8)) {
                    fprintf(stderr, "Failed to read RLE count\n");
                    exit(EXIT_FAILURE);
                }

#ifdef DEBUG
                printf("[RLE] Pattern length: %u, Repeat count: %u\n", rle_len, rle_count);
                printf("  Reading pattern bytes:\n");
#endif

                // Read the RLE pattern bytes
                for (uint32_t i = 0; i < rle_len; ++i) {
                    uint32_t temp;
                    if (!bitreader_read(reader, &temp, 8)) {
                        fprintf(stderr, "Failed to read RLE sequence byte\n");
                        exit(EXIT_FAILURE);
                    }
                    output[i] = (uint8_t)temp;
#ifdef DEBUG
                    printf("    [RLE BYTE %u] 0x%02X (%c)\n", i, output[i],
                           (output[i] >= 32 && output[i] <= 126) ? output[i] : '.');
#endif
                }

                // Write the repeated sequence
#ifdef DEBUG
                printf("  Writing %u repetitions of %u-byte pattern:\n", rle_count, rle_len);
                for (uint32_t i = 0; i < rle_len; i++) {
                    printf("    0x%02X ", output[i]);
                }
                printf("\n");
#endif

                for (uint32_t rep = 0; rep < rle_count; ++rep) {
                    fwrite(output, 1, rle_len, output_file);
#ifdef DEBUG
                    printf("    [REP %u/%u] Written\n", rep + 1, rle_count);
#endif
                }
                total_bytes_written += rle_len * rle_count;
            } else {
                // Regular compressed case (code_class 0, 1, or 2)
                uint8_t bits = get_code_class_size(code_class);
                uint32_t code;
                if (!bitreader_read(reader, &code, bits)) {
                    fprintf(stderr, "Failed to read code (%u bits)\n", bits);
                    exit(EXIT_FAILURE);
                }

#ifdef DEBUG
                printf("[COMPRESSED] Code bits: %u, Code value: %u\n", bits, code);
                bitreader_print_state(reader);
                printf("  Looking up in decoder map...\n");
#endif

                const uint8_t *seq = NULL;
                uint8_t length = 0;
                if (!decoder_map_get(&decoder_map, code, code_class, &seq, &length)) {
                    fprintf(stderr, "Failed to decode sequence for code=0x%X class=%u\n", code, code_class);
                    exit(EXIT_FAILURE);
                }

#ifdef DEBUG
                printf("  DECODED SEQUENCE: Length=%u, Bytes: ", length);
                for (uint8_t i = 0; i < length; i++) {
                    printf("0x%02X ", seq[i]);
                }
                printf("\n  Writing to output\n");
#endif

                fwrite(seq, 1, length, output_file);
                total_bytes_written += length;
            }
        }

#ifdef DEBUG
        printf("[PROGRESS] Total bytes written so far: %zu\n", total_bytes_written);
        bitreader_print_state(reader);
#endif
    }

#ifdef DEBUG
    printf("\n=== DECOMPRESSION COMPLETE ===\n");
    printf("Total bytes written: %zu\n", total_bytes_written);
    printf("Final reader state:\n");
    bitreader_print_state(reader);
#endif

    fclose(output_file);
}
// === FILE: /home/noman/takatuka/takatuka_compression/src/files/decompression//decompress_body.h ===

// decompress_header.h

#pragma once

#include "bit_reader.h"

// Read the body using the decoder map.
void read_body_using_decoder_map(BitReader* reader, const char* decompress_file_name);

// === FILE: /home/noman/takatuka/takatuka_compression/src/files/decompression//decompress.c ===

//decompress.c

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h> 
#include "decompress.h"
#include "code_classes.h"
#include "code_map.h"
#include "decompress_body.h"
#include "decompress_header.h"

#define HEADER_BUFFER_SIZE 4096

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input> <output>\n", argv[0]);
        return 1;
    }

    printf("Decompressing %s to %s...\n", argv[1], argv[2]);
    read_compressed_file(argv[1], argv[2]);
    printf("Done Decompression.\n");

    return 0;
}

void read_compressed_file(const char* input_file_name, const char* output_file_name) {
    if (!input_file_name) {
        fprintf(stderr, "Error: Invalid inputs in writeCompressedOutput\n");
        return;
    }

    FILE *file = fopen(input_file_name, "rb");
    if (!file) {
        perror("Failed to open binary reading file");
        return;
    }
    BitReader reader;
    bitreader_attach_file(&reader, file, HEADER_BUFFER_SIZE);
    
    read_header_and_create_decoder_map(&reader);   // shared reader + buffer
    printf("Read header \n");
    read_body_using_decoder_map(&reader, output_file_name);          // reuses buffer + position
    printf("Read body \n");
    bitreader_close(&reader);    
    fclose(file);
}

// === FILE: /home/noman/takatuka/takatuka_compression/src/files/decompression//decompress.h ===

//decompress.h

#pragma once
// Rest of header content

void read_compressed_file(const char* input_file_name, const char* output_file_name);

// === FILE: /home/noman/takatuka/takatuka_compression/src/files/decompression//decompress_header.c ===

// decompress_header.c

#include "decompress_header.h"
#include "bit_reader.h"
#include "decoder_map.h"
#include "code_classes.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

DecoderMap decoder_map;

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

void read_header_and_create_decoder_map(BitReader* reader) {
    uint32_t num_codes;
    SAFE_BITREAD(reader, &num_codes, 16);

#ifdef DEBUG
    printf("[DEBUG] ⏎ Read 16 bits: num_codes = %u\n", num_codes);
    bitreader_print_state(reader);
#endif

    init_decoder_map(&decoder_map, num_codes);

    for (uint32_t i = 0; i < num_codes; ++i) {
        uint32_t code_class, length, code_index;

        SAFE_BITREAD(reader, &code_class, 2);
#ifdef DEBUG
        printf("[DEBUG] ⏎ Read 2 bits: code_class = %u\n", code_class);
        bitreader_print_state(reader);
#endif

        SAFE_BITREAD(reader, &length, 8);
#ifdef DEBUG
        printf("[DEBUG] ⏎ Read 8 bits: sequence_length = %u\n", length);
        bitreader_print_state(reader);
#endif

        uint8_t class_bits = get_code_class_size((uint8_t)code_class);
        SAFE_BITREAD(reader, &code_index, class_bits);
#ifdef DEBUG
        printf("[DEBUG] ⏎ Read %u bits: code_index = %u (class %u)\n", class_bits, code_index, code_class);
        bitreader_print_state(reader);
#endif

        uint8_t* sequence = malloc(length);
        if (!sequence) {
            fprintf(stderr, "Memory allocation failure for sequence\n");
            exit(EXIT_FAILURE);
        }

        for (uint8_t j = 0; j < length; ++j) {
            uint32_t byte_val;
            SAFE_BITREAD(reader, &byte_val, 8);
            sequence[j] = (uint8_t)byte_val;
#ifdef DEBUG
            printf("[DEBUG] ⏎ Read byte #%u: %02X\n", j, sequence[j]);
            bitreader_print_state(reader);
#endif
        }

        decoder_map_set(&decoder_map, code_index, (uint8_t)code_class, sequence, (uint8_t)length);
        // Do not free sequence — it's owned by DecoderMap
    }

#ifdef DEBUG
    printf("[DEBUG] Completed DecoderMap reconstruction.\n");
    print_decoder_map(&decoder_map);
#endif
    bitreader_move_byte_boundary(reader);
}
// === FILE: /home/noman/takatuka/takatuka_compression/src/files/decompression//decompress_header.h ===

// decompress_header.h

#pragma once

#include "bit_reader.h"
#include "decoder_map.h"

extern DecoderMap decoder_map;

// Reconstructs DecoderMap by reading the header of the compressed file
void read_header_and_create_decoder_map(BitReader* reader);
