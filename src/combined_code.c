// Combined C and H Files

// === FILE: /home/noman/github/takatuka_compression/src/files/decompression/bit_reader.c ===

//bit_reader.c

#include "bit_reader.h"
#include <stdlib.h>
#include <string.h>

/* ---------- Internal helpers ---------- */

static bool br_has_file(const BitReader* br) {
    return br && br->file != NULL && br->owned_buf != NULL && br->buffer_cap > 0;
}

/* Load (or reload) the internal buffer from file. Resets byte/bit positions. */
static bool br_reload(BitReader* br) {
    if (!br_has_file(br)) return false;

    size_t n = fread(br->owned_buf, 1, br->buffer_cap, br->file);
    if (n == 0) {
        /* True EOF (or error). Set sticky overflow to signal no more data. */
        br->overflow = true;
        return false;
    }

    br->buffer      = br->owned_buf;
    br->buffer_size = n;
    br->byte_pos    = 0;
    br->bit_pos     = 0;
    /* Do not set overflow=false here; leave it as-is only if it was previously clear. */
    return true;
}

/* ---------- Public API ---------- */

void bitreader_init(BitReader *br, const uint8_t *buffer, size_t size) {
    if (!br) return;
    br->buffer      = buffer;
    br->buffer_size = size;
    br->byte_pos    = 0;
    br->bit_pos     = 0;
    br->overflow    = false;

    /* Ensure all optional fields are well-defined to avoid invalid frees later */
    br->file        = NULL;
    br->owned_buf   = NULL;
    br->buffer_cap  = 0;
}

void bitreader_attach_file(BitReader *br, FILE *file, size_t buffer_cap) {
    if (!br || !file || buffer_cap == 0) {
        fprintf(stderr, "bitreader_attach_file: invalid args\n");
        exit(EXIT_FAILURE);
    }

    /* If this reader already owns a buffer, free it first to avoid leaks */
    if (br->owned_buf) {
        free(br->owned_buf);
        br->owned_buf = NULL;
    }

    br->owned_buf = (uint8_t*)malloc(buffer_cap);
    if (!br->owned_buf) {
        fprintf(stderr, "bitreader_attach_file: OOM allocating %zu bytes\n", buffer_cap);
        exit(EXIT_FAILURE);
    }

    br->file        = file;
    br->buffer_cap  = buffer_cap;
    br->overflow    = false;

    /* Preload first chunk */
    if (!br_reload(br)) {
        /* On immediate EOF, we keep state consistent but mark overflow so reads return false */
        br->buffer      = br->owned_buf; /* valid pointer even if empty */
        br->buffer_size = 0;
        br->byte_pos    = 0;
        br->bit_pos     = 0;
    }
}

bool bitreader_read(BitReader *br, uint32_t *value, uint8_t num_bits) {
    if (!br || !value) return false;
    if (num_bits > 32)  return false;
    if (num_bits == 0) { *value = 0u; return true; }
    if (br->overflow)   return false;

    /* Save state for atomicity (restore on failure) */
    const size_t  save_byte_pos = br->byte_pos;
    const uint8_t save_bit_pos  = br->bit_pos;
    const bool    save_overflow = br->overflow;

    uint32_t out = 0;

    for (int i = (int)num_bits - 1; i >= 0; --i) {
        /* Need a byte to read a bit from */
        while (br->byte_pos >= br->buffer_size) {
            /* buffer exhausted: try to refill if we have a file */
            if (br_has_file(br)) {
                if (!br_reload(br)) {
                    /* EOF: restore and fail atomically */
                    br->byte_pos = save_byte_pos;
                    br->bit_pos  = save_bit_pos;
                    br->overflow = true; /* sticky EOF */
                    return false;
                }
            } else {
                /* no file backing; true overflow */
                br->byte_pos = save_byte_pos;
                br->bit_pos  = save_bit_pos;
                br->overflow = true;
                return false;
            }
        }

        const uint8_t cur = br->buffer[br->byte_pos];
        const uint8_t bit = (uint8_t)((cur >> (7u - br->bit_pos)) & 1u);
        out |= ((uint32_t)bit) << i;

        /* advance one bit */
        br->bit_pos++;
        if (br->bit_pos == 8) {
            br->bit_pos = 0;
            br->byte_pos++;
        }
    }

    *value = out;
    return true;
}

bool bitreader_peek(BitReader *br, uint32_t *value, uint8_t num_bits) {
    if (!br || !value) return false;
    if (num_bits > 32)  return false;
    if (num_bits == 0) { *value = 0u; return true; }
    if (br->overflow)   return false;

    /* Save state, do NOT auto-refill for peek */
    const size_t  save_byte_pos = br->byte_pos;
    const uint8_t save_bit_pos  = br->bit_pos;
    const bool    save_overflow = br->overflow;

    uint32_t out = 0;

    for (int i = (int)num_bits - 1; i >= 0; --i) {
        if (br->byte_pos >= br->buffer_size) {
            /* Not enough data in the currently loaded buffer */
            br->byte_pos = save_byte_pos;
            br->bit_pos  = save_bit_pos;
            br->overflow = save_overflow;
            return false;
        }
        const uint8_t cur = br->buffer[br->byte_pos];
        const uint8_t bit = (uint8_t)((cur >> (7u - br->bit_pos)) & 1u);
        out |= ((uint32_t)bit) << i;

        /* advance one bit locally */
        br->bit_pos++;
        if (br->bit_pos == 8) {
            br->bit_pos = 0;
            br->byte_pos++;
        }
    }

    /* Restore state */
    br->byte_pos = save_byte_pos;
    br->bit_pos  = save_bit_pos;
    br->overflow = save_overflow;

    *value = out;
    return true;
}

void bitreader_move_byte_boundary(BitReader *br) {
    if (!br) return;
    if (br->bit_pos != 0) {
        br->bit_pos = 0;
        br->byte_pos++;
    }
    /* If we landed past the end of the loaded buffer, the next read() will refill automatically. */
}

uint8_t *bitreader_load_from_file(FILE *fp, size_t *out_size) {
    if (!fp) return NULL;

    /* Use fseeko/ftello when available; fallback to ftell on platforms without large-file API */
#if defined(_WIN32) || defined(_WIN64)
    /* Windows ftell is 32-bit on old MSVCRT; assume modern runtime or large-file not critical here */
    if (fseek(fp, 0, SEEK_END) != 0) return NULL;
    long sz = ftell(fp);
    if (sz < 0) return NULL;
    if (fseek(fp, 0, SEEK_SET) != 0) return NULL;
    size_t size = (size_t)sz;
#else
    if (fseeko(fp, 0, SEEK_END) != 0) return NULL;
    off_t sz = ftello(fp);
    if (sz < 0) return NULL;
    if (fseeko(fp, 0, SEEK_SET) != 0) return NULL;
    size_t size = (size_t)sz;
#endif

    uint8_t *buf = (uint8_t*)malloc(size ? size : 1); /* malloc(0) is implementation-defined */
    if (!buf) return NULL;

    size_t n = fread(buf, 1, size, fp);
    if (n != size) {
        free(buf);
        return NULL;
    }

    if (out_size) *out_size = size;
    return buf;
}

bool bitreader_fill_next_chunk(BitReader *br) {
    if (!br) return false;
    if (!br_has_file(br)) return false;

    /* Only allow manual refill when we are exactly at a byte boundary;
       if not, the caller should first call bitreader_move_byte_boundary(). */
    if (br->bit_pos != 0) {
        /* Not an error; just refuse to refill mid-byte to avoid state confusion. */
        return false;
    }

    return br_reload(br);
}

void bitreader_print_state(const BitReader *br) {
    if (!br) return;
    printf("[BitReader] byte_pos=%zu bit_pos=%u buffer_size=%zu overflow=%s file=%p cap=%zu\n",
           br->byte_pos, br->bit_pos, br->buffer_size,
           br->overflow ? "true" : "false", (void*)br->file, br->buffer_cap);
}

void bitreader_reset(BitReader *br, const uint8_t *new_buffer, size_t new_size) {
    if (!br) return;

    /* If previously attached to a file, keep ownership of owned_buf, but switch to external buffer mode. */
    br->buffer      = new_buffer;
    br->buffer_size = new_size;
    br->byte_pos    = 0;
    br->bit_pos     = 0;
    br->overflow    = false;

    /* Detach file semantics (do not free here) */
    br->file        = NULL;
    br->buffer_cap  = 0;
    /* Keep owned_buf allocated in case user re-attaches; but since file is NULL now, we won't free/use it. */
    if (br->owned_buf) {
        free(br->owned_buf);
        br->owned_buf = NULL;
    }
}

void bitreader_close(BitReader *br) {
    if (!br) return;

    if (br->owned_buf) {
        free(br->owned_buf);
        br->owned_buf = NULL;
    }

    /* Do not fclose(br->file) here — the caller owns the FILE* */
    br->buffer      = NULL;
    br->buffer_size = 0;
    br->byte_pos    = 0;
    br->bit_pos     = 0;
    br->overflow    = false;

    br->file        = NULL;
    br->buffer_cap  = 0;
}

// === FILE: /home/noman/github/takatuka_compression/src/files/decompression/bit_reader.h ===

// bit_reader.h =====

#pragma once


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>



/*
* Robust BitReader with streaming support.
*
* - Supports attaching to a FILE* and reading in chunks (owned_buf).
* - Preserves partial-byte state across chunk refills: if a read stops in the
* middle of a byte, the remaining bits of that byte are preserved and used
* when the next chunk is loaded.
* - bitreader_read(...) will automatically attempt to refill from the file
* when the current in-memory buffer runs out.
* - bitreader_peek(...) does NOT trigger a refill: it only peeks within the
* currently loaded buffer (safe for quick lookahead without side effects).
*
* Usage notes:
* - Call bitreader_attach_file() to set up streaming from a file. The initial
* chunk is read immediately.
* - When bitreader_read() returns false, check br->overflow or errno if you
* need diagnostics. If it returns false in the middle of a large read it's
* usually EOF.
*/


typedef struct {
    /* Current readable window */
    const uint8_t* buffer;   /* points to owned_buf when attached to a file; otherwise external */
    size_t         buffer_size;  /* bytes currently valid in buffer */
    size_t         byte_pos;     /* current byte index in buffer */
    uint8_t        bit_pos;      /* current bit index in current byte: 0 (MSB)..7 (LSB) */

    /* Error/EOF state */
    bool           overflow;     /* sticky error: set on unrecoverable failure */

    /* Optional backing file + owned staging buffer */
    FILE*          file;         /* non-NULL if attached to a file */
    uint8_t*       owned_buf;    /* staging buffer we own when file-attached */
    size_t         buffer_cap;   /* capacity of owned_buf in bytes */
} BitReader;

/* Attach to an existing memory buffer (we do not take ownership) */
void bitreader_init(BitReader* br, const uint8_t* buffer, size_t size);

/* Attach to a FILE* and allocate an internal buffer of capacity buffer_cap, preloading the first chunk */
void bitreader_attach_file(BitReader* br, FILE* file, size_t buffer_cap);

/* Read num_bits (<=32) into *value; returns true on success, false on (true) EOF/error.
   This function auto-refills from FILE* as needed and is atomic (state restored on failure). */
bool bitreader_read(BitReader* br, uint32_t* value, uint8_t num_bits);

/* Non-destructive lookahead inside the currently loaded buffer only (no auto-refill). */
bool bitreader_peek(BitReader* br, uint32_t* value, uint8_t num_bits);

/* Force-aligned to next byte; if at end of buffer, the next read will refill automatically. */
void bitreader_move_byte_boundary(BitReader* br);

/* Replace the in-memory buffer (no ownership taken). Resets position and clears errors. */
void bitreader_reset(BitReader* br, const uint8_t* new_buffer, size_t new_size);

/* Utility: read entire FILE into a newly malloc’d buffer. Caller owns/free(). */
uint8_t* bitreader_load_from_file(FILE* fp, size_t* out_size);

/* Manual chunk refill (mainly for specialized uses/tests). Returns true if more data was loaded. */
bool bitreader_fill_next_chunk(BitReader* br);

/* Debug helper */
void bitreader_print_state(const BitReader* br);

/* Release internal resources (owned buffer). Safe to call regardless of how the reader was initialized. */
void bitreader_close(BitReader* br);

// === FILE: /home/noman/github/takatuka_compression/src/files/decompression/decoder_map.c ===

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

// === FILE: /home/noman/github/takatuka_compression/src/files/decompression/decoder_map.h ===

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

// === FILE: /home/noman/github/takatuka_compression/src/files/decompression/decompress_body.c ===

//decompress_body.c

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

            if (code_class == 3) { // RLE
                uint32_t rle_len = 1;
                /*if (!bitreader_read(reader, &rle_len, 3)) {
                    fprintf(stderr, "Failed to read RLE length\n");
                    exit(EXIT_FAILURE);
                }*/

                uint32_t rle_count;
                if (!bitreader_read(reader, &rle_count, 8)) {
                    fprintf(stderr, "Failed to read RLE count\n");
                    exit(EXIT_FAILURE);
                }

                for (uint32_t i = 0; i < rle_len; ++i) {
                    uint32_t temp;
                    if (!bitreader_read(reader, &temp, 8)) {
                        fprintf(stderr, "Failed to read RLE sequence byte\n");
                        exit(EXIT_FAILURE);
                    }
                    output[i] = (uint8_t)temp;
                }

                for (uint32_t rep = 0; rep < rle_count; ++rep) {
                    fwrite(output, 1, rle_len, output_file);
                }
                total_bytes_written += rle_len * rle_count;
            } else {
                // Regular compressed case (class 0,1 or 2)
                /* Use the dynamic class2 bit-width read from header */
                uint8_t bits = get_code_class_size((uint8_t)code_class, global_class2_bits);

                uint32_t code;
                if (!bitreader_read(reader, &code, bits)) {
                    fprintf(stderr, "Failed to read code (%u bits)\n", bits);
                    exit(EXIT_FAILURE);
                }

                const uint8_t *seq = NULL;
                uint8_t length = 0;
                if (!decoder_map_get(&decoder_map, (uint16_t)code, (uint8_t)code_class, &seq, &length)) {
                    fprintf(stderr, "Failed to decode sequence for code=0x%X class=%u\n", code, code_class);
                    exit(EXIT_FAILURE);
                }

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

// === FILE: /home/noman/github/takatuka_compression/src/files/decompression/decompress_body.h ===

// decompress_header.h

#pragma once

#include "bit_reader.h"

// Read the body using the decoder map.
void read_body_using_decoder_map(BitReader* reader, const char* decompress_file_name);

// === FILE: /home/noman/github/takatuka_compression/src/files/decompression/decompress.c ===

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

// === FILE: /home/noman/github/takatuka_compression/src/files/decompression/decompress.h ===

//decompress.h

#pragma once
// Rest of header content

void read_compressed_file(const char* input_file_name, const char* output_file_name);

// === FILE: /home/noman/github/takatuka_compression/src/files/decompression/decompress_header.c ===

// decompress_header.c
//
// Decoder for the compact header format produced by the updated compressor.
// Header layout (bit-packed, no unnecessary padding):
//  - 16 bits : total number of codewords (N)
//  - 8  bits : class2_bits (B2)      -- present iff N > 0
//  - 8  bits : len_bits[0]           -- number of bits used to encode (length-1) for class 0 entries
//  - 8  bits : len_bits[1]
//  - 8  bits : len_bits[2]
//  - Then entries in class order: first all Class0 entries, then Class1, then Class2.
//    For each entry (index order within class):
//      - if len_bits[class] > 0 : read length_minus_one using len_bits[class] bits and decode length = length_minus_one + 1
//      - else (len_bits[class] == 0): length = 1 (no bits stored)
//      - read `length` bytes (8 bits each) for the sequence
//
// Notes:
//  * No per-entry class or index fields are stored — index is the ordinal position inside that class.
//  * The decoder derives the number of entries per class from N and class capacities:
//      n0 = min(N, C0)
//      n1 = min(max(N - n0, 0), C1)
//      n2 = N - n0 - n1
//    where C0 = get_code_class_threshold(0, 0) (fixed), C1 likewise, and C2 = get_code_class_threshold(2, B2).
//  * The only byte boundary alignment performed is a final call to bitreader_move_byte_boundary()
//    after header parsing so the body reader can start aligned if necessary.  All header fields are
//    read bit-packed — this keeps the header minimal.

#include "decompress_header.h"
#include "bit_reader.h"
#include "decoder_map.h"
#include "code_classes.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>

/* Global decoder map and the class2 bit-width for this file (exported in header) */
DecoderMap decoder_map;
uint8_t global_class2_bits = 0; /* set by read_header_and_create_decoder_map() */

/* Macro: attempt to read `bits` bits; if bitreader_read fails, try to refill buffer and retry.
   On persistent failure, print message and exit. */
#define SAFE_BITREAD(reader_ptr, out_var, bits)                                         \
    do {                                                                                 \
        if (!bitreader_read((reader_ptr), (out_var), (bits))) {                          \
            /* try refill and retry once */                                              \
            if (!bitreader_fill_next_chunk((reader_ptr))) {                              \
                fprintf(stderr, "Failed to read %u bits (EOF/overflow)\\n", (unsigned)(bits)); \
                exit(EXIT_FAILURE);                                                       \
            }                                                                            \
            if (!bitreader_read((reader_ptr), (out_var), (bits))) {                      \
                fprintf(stderr, "bitreader_read failed after refill for %u bits\\n", (unsigned)(bits)); \
                exit(EXIT_FAILURE);                                                       \
            }                                                                            \
        }                                                                                \
    } while (0)


/* Helper to derive per-class counts from total N using the encoder's greedy policy. */
static void derive_class_counts(uint16_t N, uint16_t cap0, uint16_t cap1, uint16_t cap2,
                                uint16_t out_counts[3]) {
    uint16_t n0 = (N < cap0) ? N : cap0;
    uint16_t rem = (N > n0) ? (N - n0) : 0;
    uint16_t n1 = (rem < cap1) ? rem : cap1;
    uint16_t n2 = (N > (n0 + n1)) ? (N - n0 - n1) : 0;
    out_counts[0] = n0;
    out_counts[1] = n1;
    out_counts[2] = n2;
}

/* Main header reader */
void read_header_and_create_decoder_map(BitReader* reader) {
    if (!reader) {
        fprintf(stderr, "read_header_and_create_decoder_map: reader == NULL\n");
        exit(EXIT_FAILURE);
    }

    uint32_t tmp_n = 0;
    SAFE_BITREAD(reader, &tmp_n, 16);
    if (tmp_n >= 0x10000u) {
        fprintf(stderr, "Header: invalid num_codes >= 2^16: %u\n", tmp_n);
        exit(EXIT_FAILURE);
    }
    uint16_t num_codes = (uint16_t)tmp_n;

#ifdef DEBUG
    printf("[DEBUG] Read num_codes = %u\n", (unsigned)num_codes);
    bitreader_print_state(reader);
#endif

    /* Initialize decoder map with capacity for num_codes */
    init_decoder_map(&decoder_map, num_codes);

    /* If no codes, encoder wrote only the 16-bit placeholder and nothing else */
    if (num_codes == 0) {
        global_class2_bits = 0;
#ifdef DEBUG
        printf("[DEBUG] No codes in header; set global_class2_bits = 0\n");
#endif
        /* Align to next byte boundary so body reader starts clean (encoder's writer may have byte_tail) */
        bitreader_move_byte_boundary(reader);
        return;
    }

    /* Read class2_bits (one byte) */
    uint32_t cb = 0;
    SAFE_BITREAD(reader, &cb, 8);
    global_class2_bits = (uint8_t)cb;

#ifdef DEBUG
    printf("[DEBUG] Read class2_bits = %u\n", (unsigned)global_class2_bits);
    bitreader_print_state(reader);
#endif

    /* Read per-class len_bits (one byte each) */
    uint32_t lb0 = 0, lb1 = 0, lb2 = 0;
    SAFE_BITREAD(reader, &lb0, 8);
    SAFE_BITREAD(reader, &lb1, 8);
    SAFE_BITREAD(reader, &lb2, 8);

    uint8_t len_bits[3];
    len_bits[0] = (uint8_t)lb0;
    len_bits[1] = (uint8_t)lb1;
    len_bits[2] = (uint8_t)lb2;

#ifdef DEBUG
    printf("[DEBUG] Read len_bits = [%u, %u, %u]\n", (unsigned)len_bits[0], (unsigned)len_bits[1], (unsigned)len_bits[2]);
    bitreader_print_state(reader);
#endif

    /* Derive per-class capacities and counts */
    uint16_t cap0 = get_code_class_threshold(0, 0); /* fixed */
    uint16_t cap1 = get_code_class_threshold(1, 0); /* fixed */
    uint16_t cap2 = get_code_class_threshold(2, global_class2_bits); /* dynamic */

    uint16_t counts[3];
    derive_class_counts(num_codes, cap0, cap1, cap2, counts);

    if (counts[2] > cap2) {
        fprintf(stderr, "Header error: class2 needs %u entries but capacity is %u (class2_bits=%u)\n",
                (unsigned)counts[2], (unsigned)cap2, (unsigned)global_class2_bits);
        exit(EXIT_FAILURE);
    }

#ifdef DEBUG
    printf("[DEBUG] Derived class counts: n0=%u, n1=%u, n2=%u (caps: %u,%u,%u)\n",
           (unsigned)counts[0], (unsigned)counts[1], (unsigned)counts[2],
           (unsigned)cap0, (unsigned)cap1, (unsigned)cap2);
#endif

    /* Read entries in class order (0,1,2); index within class = ordinal position */
    for (int cls = 0; cls <= 2; ++cls) {
        uint16_t n_here = counts[cls];
        uint8_t lb = len_bits[cls];

        for (uint16_t idx = 0; idx < n_here; ++idx) {
            uint32_t length = 0;

            if (lb > 0) {
                uint32_t lminus;
                SAFE_BITREAD(reader, &lminus, lb);
                /* lminus is length-1 */
                length = lminus + 1;
            } else {
                /* len_bits == 0 => encoder omitted length field; that implies length == 1 */
                length = 1;
            }

            if (length == 0) {
                fprintf(stderr, "Header error: decoded zero length for class %d index %u\n", cls, (unsigned)idx);
                exit(EXIT_FAILURE);
            }

            /* allocate sequence buffer */
            uint8_t *seq = (uint8_t*)malloc((size_t)length);
            if (!seq) {
                fprintf(stderr, "OOM allocating %u bytes for sequence (class %d index %u)\n", (unsigned)length, cls, (unsigned)idx);
                exit(EXIT_FAILURE);
            }

            /* read sequence bytes (each 8 bits) */
            for (uint32_t b = 0; b < length; ++b) {
                uint32_t bv;
                SAFE_BITREAD(reader, &bv, 8);
                seq[b] = (uint8_t)bv;
            }

            /* store mapping: index-within-class = idx, class = cls */
            /* Keep parameter order same as existing decoder_map_set usage */
            if (!decoder_map_set(&decoder_map, (uint16_t)idx, (uint8_t)cls, seq, (uint8_t)length)) {
                fprintf(stderr, "decoder_map_set failed for class=%u idx=%u\n", (unsigned)cls, (unsigned)idx);
                exit(EXIT_FAILURE);
            }
            /* decoder_map_set assumed to take ownership of seq */
        }
    }

#ifdef DEBUG
    printf("[DEBUG] Completed DecoderMap reconstruction (num_codes=%u)\n", (unsigned)num_codes);
    print_decoder_map(&decoder_map);
#endif

    /* Align to next byte boundary so the body reader starts at a byte boundary (if needed) */
    bitreader_move_byte_boundary(reader);
}

// === FILE: /home/noman/github/takatuka_compression/src/files/decompression/decompress_header.h ===

// decompress_header.h

#pragma once

#include "bit_reader.h"
#include "decoder_map.h"
#include <stdint.h>

/* decoder map reconstructed from header */
extern DecoderMap decoder_map;

/*
 * Number of bits used for class-2 indices in this file.
 * This is read from the file header (1 byte) and used by the body reader.
 *  - 0 means "no class2 codes present" (or class2_count==1 case where encoder used 0 bits).
 */
extern uint8_t global_class2_bits;

/* Reconstructs DecoderMap by reading the header of the compressed file */
void read_header_and_create_decoder_map(BitReader* reader);
