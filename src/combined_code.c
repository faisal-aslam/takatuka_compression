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
    if (code_class > 3) {
        fprintf(stderr, "Invalid code_class %d Exiting (get_header_overhead)!\n", code_class);
        exit(EXIT_FAILURE);
        return 0;
    }
    if (code_class == 3) {        
        /* RLE is handled outside the codebook; no index bits here. */
        return 0; 
    }
    uint8_t code_bits = get_code_class_size(code_class, class2_bits);
    return (uint8_t)(3 + (seq_length * 8) + 2 + code_bits);
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
        case 0: return 5;           /* fixed */
        case 1: return 7;           /* fixed */
        case 2: return class2_bits; /* dynamic */
        case 3: return 0;           /* RLE: no index bits - ADD THIS LINE */
        default:
            fprintf(stderr, "Invalid code_class %d Exiting (get_code_class_size)!\n", code_class);
            exit(EXIT_FAILURE);
    }
}

/* Return threshold (capacity) for given class as uint16_t. For class2 uses class2_bits. */
uint16_t get_code_class_threshold(uint8_t code_class, uint8_t class2_bits) {
    switch (code_class) {
        case 0: return (uint16_t)(1u << get_code_class_size(0, class2_bits));
        case 1: return (uint16_t)(1u << get_code_class_size(1, class2_bits));
        case 2: {
            if (class2_bits >= 16) {
                fprintf(stderr, "Requested threshold bits too large: %u\n", class2_bits);
                exit(EXIT_FAILURE);
            }
            return (uint16_t)(1u << class2_bits);
        }
        case 3: return 0; /* RLE: there are no codebook entries */
        default:
            fprintf(stderr, "Invalid code_class %d Exiting (get_code_class_threshold)!\n", code_class);
            exit(EXIT_FAILURE);
    }
}

/* Returns overhead of a code_class (kept same as before) */
uint8_t get_code_class_overhead(uint8_t code_class) {
    (void)code_class; // unused for now
    return 3;
}

// === FILE: /home/noman/takatuka/takatuka_compression/src/files/compression/bit_writer.c ===

// bit_writer.c — robust, all-or-nothing bitstream writer with growable buffer.

#include "bit_writer.h"
#include <stdlib.h>
#include <string.h>

/* ---------- internal helpers ---------- */

static inline size_t bw_total_bits(const BitWriter *bw) { return (bw->byte_pos * 8u) + (size_t)bw->bit_pos; }

static bool bw_reserve_bytes(BitWriter *bw, size_t needed_bytes) {
    if (needed_bytes <= bw->_cap) return true;

    size_t new_cap = bw->_cap ? bw->_cap : (bw->buffer_size ? bw->buffer_size : 4096u);
    while (new_cap < needed_bytes) {
        size_t doubled = new_cap * 2u;
        if (doubled < new_cap) { // overflow guard
            new_cap = needed_bytes;
            break;
        }
        new_cap = doubled;
    }

    uint8_t *new_data = (uint8_t *)realloc(bw->_data, new_cap);
    if (!new_data) return false;

    /* zero the newly added portion to keep bitwise ops deterministic */
    if (new_cap > bw->_cap) {
        memset(new_data + bw->_cap, 0, new_cap - bw->_cap);
    }

    bw->_data = new_data;
    bw->_cap = new_cap;
    return true;
}

static inline void bw_update_sizes_after_cursor_move(BitWriter *bw) {
    /* _size should reflect the number of whole bytes that contain any data */
    size_t used = bw->byte_pos + (bw->bit_pos != 0 ? 1u : 0u);
    if (used > bw->_size) {
        /* zero the new byte if we just extended into it */
        if (bw->_data && used > 0 && used > bw->_size) {
            /* ensure the current target byte is zeroed (already zeroed in reserve, but safe) */
            /* not strictly needed due to reserve zeroing, but harmless */
        }
        bw->_size = used;
    }
}

/* ---------- public API ---------- */

void bitwriter_init(BitWriter *bw, uint8_t *buffer, size_t size) {
    if (!bw) return;

    bw->buffer = buffer;    /* legacy, not used for storage */
    bw->buffer_size = size; /* legacy, not used for storage */
    bw->byte_pos = 0;
    bw->bit_pos = 0;
    bw->overflow = false;

    bw->_data = NULL;
    bw->_cap = 0;
    bw->_size = 0;

    /* pre-reserve based on provided size (or 4096) */
    size_t initial = size ? size : 4096u;
    if (bw_reserve_bytes(bw, initial)) {
        /* zero initial area */
        memset(bw->_data, 0, bw->_cap);
    }
#ifdef DEBUG
    /* mirror original behavior: zero the legacy external buffer if provided */
    if (buffer && size) {
        memset(buffer, 0, size);
    }
#endif
}

bool bitwriter_write(BitWriter *bw, uint32_t value, uint8_t num_bits
#ifdef DEBUG
                     ,
                     const char *label
#endif
) {
    if (!bw) return false;
    if (num_bits == 0) return true;
    if (num_bits > 32) {
        bw->overflow = true;
        return false;
    }
#ifdef DEBUG
    FILE *log = fopen("log.txt", "a");
    if (log) {
        if (label) fprintf(log, "[BitWriter] %s: ", label);
        else
            fprintf(log, "[BitWriter] ");
        fprintf(log, "Writing %u bits: ", num_bits);
    }
#endif

    /* compute target capacity in bytes BEFORE writing to ensure all-or-nothing */
    size_t start_bits = bw_total_bits(bw);
    size_t target_bits = start_bits + (size_t)num_bits;
    size_t needed_bytes = (target_bits + 7u) / 8u;
    if (!bw_reserve_bytes(bw, needed_bytes)) {
#ifdef DEBUG
        if (log) {
            fprintf(log, " (ALLOC FAIL)\n");
            fclose(log);
        }
#endif
        return false;
    }

    /* perform the write, guaranteed to fit */
    for (int i = (int)num_bits - 1; i >= 0; --i) {
        uint8_t bit = (uint8_t)((value >> i) & 1u);

        /* ensure the target byte exists (reserve already zeroed it) */
        if (bit) {
            bw->_data[bw->byte_pos] |= (uint8_t)(1u << (7 - bw->bit_pos));
        } else {
            bw->_data[bw->byte_pos] &= (uint8_t)~(1u << (7 - bw->bit_pos));
        }

#ifdef DEBUG
        if (log) fputc(bit ? '1' : '0', log);
#endif

        bw->bit_pos++;
        if (bw->bit_pos == 8) {
            bw->bit_pos = 0;
            bw->byte_pos++;
        }
    }

    bw_update_sizes_after_cursor_move(bw);

#ifdef DEBUG
    if (log) {
        fputc('\n', log);
        fclose(log);
    }
#endif
    return true;
}

size_t bitwriter_bytes_written(const BitWriter *bw) { return bw ? bw->_size : 0u; }

bool bitwriter_write_to_file(const BitWriter *bw_in, FILE *fp) {
    if (!bw_in || !fp) return false;

    /* We need a non-const handle to reset/clear internal storage after writing. */
    BitWriter *bw = (BitWriter *)bw_in;

    size_t bytes_to_write = bitwriter_bytes_written(bw);
    if (bytes_to_write == 0) return true;

    size_t written = fwrite(bw->_data, 1, bytes_to_write, fp);
    if (written != bytes_to_write) {
        return false;
    }

    /* After a successful flush, clear the stream and release memory to avoid leaks. */
    bw->byte_pos = 0;
    bw->bit_pos = 0;
    bw->overflow = false;

    if (bw->_data) {
        free(bw->_data);
        bw->_data = NULL;
    }
    bw->_cap = 0;
    bw->_size = 0;

    return true;
}

void bitwriter_print_state(const BitWriter *bw) {
    if (!bw) return;
    size_t total_bits = bw_total_bits(bw);
    printf("[BitWriter] byte_pos = %zu, bit_pos = %u, total_bits = %zu, overflow = %s, bytes_buffered = %zu\n",
           bw->byte_pos, bw->bit_pos, total_bits, bw->overflow ? "true" : "false", bitwriter_bytes_written(bw));
}

bool bitwriter_overwrite_at(BitWriter *bw, size_t bit_pos, uint32_t value, uint8_t num_bits) {
    if (!bw) return false;
    if (num_bits == 0) return true;
    if (num_bits > 32) {
        bw->overflow = true;
        return false;
    }

    size_t total_stream_bits = bw->_size * 8u;
    if (bit_pos + (size_t)num_bits > total_stream_bits) {
        /* cannot patch beyond current stream end */
        return false;
    }

    for (int i = (int)num_bits - 1; i >= 0; --i) {
        size_t current_bit = bit_pos + (size_t)(num_bits - 1 - i);
        size_t byte_index = current_bit / 8u;
        size_t bit_index = 7u - (current_bit % 8u); // MSB-first
        uint8_t bit = (uint8_t)((value >> i) & 1u);

        if (bit) bw->_data[byte_index] |= (uint8_t)(1u << bit_index);
        else
            bw->_data[byte_index] &= (uint8_t)~(1u << bit_index);
    }
    return true;
}

void bitwriter_reset(BitWriter *bw) {
    if (!bw) return;
    bw->byte_pos = 0;
    bw->bit_pos = 0;
    bw->overflow = false;

    if (bw->_data) {
        free(bw->_data);
        bw->_data = NULL;
    }
    bw->_cap = 0;
    bw->_size = 0;

    /* keep legacy external buffer zeroed if provided; it is not used for storage */
    if (bw->buffer && bw->buffer_size) {
        memset(bw->buffer, 0, bw->buffer_size);
    }
}

void bitwriter_reset_positions(BitWriter *bw) {
    if (!bw) return;
    /* clear stream but keep capacity for reuse */
    bw->byte_pos = 0;
    bw->bit_pos = 0;
    bw->overflow = false;

    if (bw->_data && bw->_size) {
        /* zero the used region for cleanliness */
        memset(bw->_data, 0, bw->_size);
    }
    bw->_size = 0;
}

// === FILE: /home/noman/takatuka/takatuka_compression/src/files/compression/bit_writer.h ===

// bit_writer.h

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/*
 * BitWriter — robust, all-or-nothing bitstream writer.
 *
 * Public fields kept for compatibility with existing code:
 *   - buffer / buffer_size are accepted by init but not used for storage;
 *     the writer manages its own dynamic buffer internally.
 *   - byte_pos / bit_pos reflect the current end-of-stream cursor.
 *   - overflow is maintained for API compatibility; it is set only for
 *     invalid inputs (e.g., num_bits > 32), never for capacity growth.
 *
 * Key guarantees:
 *   - bitwriter_write(): either writes the entire value OR returns false
 *     without modifying the stream (no partial writes).
 *   - bitwriter_overwrite_at(): patches bits anywhere in the current
 *     in-memory stream (absolute bit positions from 0).
 *   - bitwriter_write_to_file(): writes all buffered bytes to FILE* and
 *     then clears/releases the internal buffer so there are no leaks.
 */

typedef struct {
    /* legacy / public fields (kept for external code that reads them) */
    uint8_t *buffer;    /* not used for storage; preserved for ABI */
    size_t buffer_size; /* not used for storage; preserved for ABI */
    size_t byte_pos;    /* current write byte position (end of stream) */
    uint8_t bit_pos;    /* next bit position within current byte [0..7] */
    bool overflow;      /* set only on invalid input */

    /* internal dynamic storage */
    uint8_t *_data; /* owned contiguous byte buffer */
    size_t _cap;    /* allocated capacity in bytes */
    size_t _size;   /* bytes currently used (== bytes written so far) */
} BitWriter;

#ifdef __cplusplus
extern "C" {
#endif

void bitwriter_init(BitWriter *bw, uint8_t *buffer, size_t size);
size_t bitwriter_bytes_written(const BitWriter *bw);
bool bitwriter_write_to_file(const BitWriter *bw, FILE *fp);
void bitwriter_print_state(const BitWriter *bw);
bool bitwriter_overwrite_at(BitWriter *bw, size_t bit_pos, uint32_t value, uint8_t num_bits);
void bitwriter_reset(BitWriter *bw);           /* clear stream + free internal storage */
void bitwriter_reset_positions(BitWriter *bw); /* clear stream, keep capacity for reuse */

bool bitwriter_write(BitWriter *bw, uint32_t value, uint8_t num_bits
#ifdef DEBUG
                     ,
                     const char *label
#endif
);

#ifdef __cplusplus
}
#endif

// === FILE: /home/noman/takatuka/takatuka_compression/src/files/compression/code_map.c ===

//code_map.c

#include "code_map.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>  // For printf
#include "xxhash.h"
#include "general_map.h"

void init_code_map(CodeMap* map, size_t capacity) {
    map->capacity = capacity * 2; // keep load factor ≤ 0.5
    map->size = 0;
    map->entries = calloc(map->capacity, sizeof(CodeMapEntry));
}

void free_code_map(CodeMap* map) {
    free(map->entries);
    map->entries = NULL;
    map->capacity = 0;
    map->size = 0;
}

static inline uint64_t hash_seq(const uint8_t* seq, uint8_t len) {
    return XXH3_64bits(seq, len);
}

static inline size_t probe_index(uint64_t hash, size_t i, size_t cap) {
    return (hash + i) % cap;
}

bool code_map_set(CodeMap* map, const uint8_t* seq, uint8_t len, uint16_t code, uint8_t code_class) {
    if (map->size >= map->capacity / 2) {
        return false; // Map is too full (load factor > 0.5)
    }

    uint64_t h = hash_seq(seq, len);
    for (size_t i = 0; i < map->capacity; ++i) {
        size_t idx = probe_index(h, i, map->capacity);
        CodeMapEntry* e = &map->entries[idx];

        if (!e->occupied) {
            e->seq = seq;
            e->length = len;
            e->code = code;
            e->code_class = code_class;
            e->hash = h;
            e->occupied = true;
            map->size++;
            return true;
        }

        if (e->hash == h && e->length == len && sequences_equal(e->seq, seq, len)) {
            e->code = code;
            e->code_class = code_class;
            return true; // update existing entry
        }
    }
    return false; // shouldn't reach here if load factor is maintained
}

bool code_map_get(const CodeMap* map, const uint8_t* seq, uint8_t len, uint16_t* out_code, uint8_t* out_class) {
    uint64_t h = hash_seq(seq, len);
    for (size_t i = 0; i < map->capacity; ++i) {
        size_t idx = probe_index(h, i, map->capacity);
        const CodeMapEntry* e = &map->entries[idx];

        if (!e->occupied) return false;
        if (e->hash == h && e->length == len && sequences_equal(e->seq, seq, len)) {
            *out_code = e->code;
            *out_class = e->code_class;
            return true;
        }
    }
    return false;
}

void print_code_map(const CodeMap* map) {
    printf("CodeMap (size: %zu, capacity: %zu):\n", map->size, map->capacity);
    printf("-------------------------------------------------\n");
    printf("| Index | Occupied | Length | Code | Class | Hash (low 32) | Sequence\n");
    printf("-------------------------------------------------\n");
    
    for (size_t i = 0; i < map->capacity; ++i) {
        const CodeMapEntry* e = &map->entries[i];
        printf("| %5zu | %8s | %6u | %4u | %5u | %11llX | ", 
               i, 
               e->occupied ? "true" : "false",
               e->occupied ? e->length : 0,
               e->occupied ? e->code : 0,
               e->occupied ? e->code_class : 0,
               e->occupied ? (unsigned long long)(e->hash & 0xFFFFFFFF) : 0);
        
        if (e->occupied) {
            for (uint8_t j = 0; j < e->length; ++j) {
                printf("%c", e->seq[j]);
            }
        } else {
            printf("(empty)");
        }
        printf("\n");
    }
    printf("-------------------------------------------------\n");
}
// === FILE: /home/noman/takatuka/takatuka_compression/src/files/compression/code_map.h ===

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    const uint8_t* seq;  // pointer to external block memory (not owned)
    uint8_t length;
    uint16_t code;
    uint8_t code_class;
    uint64_t hash;
    bool occupied;
} CodeMapEntry;

typedef struct {
    CodeMapEntry* entries;
    size_t capacity;
    size_t size;
} CodeMap;

void init_code_map(CodeMap* map, size_t capacity);
void free_code_map(CodeMap* map);
bool code_map_set(CodeMap* map, const uint8_t* seq, uint8_t len, uint16_t code, uint8_t code_class);
bool code_map_get(const CodeMap* map, const uint8_t* seq, uint8_t len, uint16_t* out_code, uint8_t* out_class);
void print_code_map(const CodeMap* map);  // New debug function


// === FILE: /home/noman/takatuka/takatuka_compression/src/files/compression/compress.c ===

//compress.c main file to write compressed data

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h> 
#include "compress.h"
#include "code_classes.h"
#include "best_path_view.h"
#include "compressed_header.h"
#include "code_map.h"
#include "compressed_body.h"
#include "bit_writer.h"

#define BUFFER_SIZE 4096


/**
  * @brief Main function to write complete compressed output file
  * 
  * @param filename Output file path
  * @param sequences Array of binary sequences for header
  * @param seq_count Number of sequences
  * @param best_node TreeNode with best compression path
  * @param block Pointer to raw data block
  */
long write_compressed_output(const char* filename, const uint8_t* block) {
    if (!filename || !block) {
        fprintf(stderr, "Error: Invalid inputs in writeCompressedOutput\n");
        return 0;
    }

    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open output file");
        return 0;
    }

 #ifdef DEBUG
    printf("[DEBUG] Initial best path view:\n");
    print_best_view(&view, 1, block);
#endif

    printf("\n ==== Starting compressed output writing === \n");

    // Allocate a single buffer for both header and body
    size_t buffer_size = BUFFER_SIZE;
    uint8_t* buffer = malloc(buffer_size);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate buffer\n");
        fclose(file);
        return 0;
    }

    BitWriter writer;
    bitwriter_init(&writer, buffer, buffer_size);

#ifdef DEBUG
    printf("[DEBUG] Initialized BitWriter with buffer size: %zu\n", buffer_size);
#endif

    // Write header
    long header_size = populate_header(view, block, file, &writer);

#ifdef DEBUG
    printf("[DEBUG] After header writing:\n");
    print_best_view(&view, 1, block);
    printf("[DEBUG] BitWriter state after header:\n");
    bitwriter_print_state(&writer);
#endif

    // Write body
    long body_size = populate_body(view, block, file, &writer);

#ifdef DEBUG
    printf("[DEBUG] BitWriter state after body:\n");
    bitwriter_print_state(&writer);
#endif

    printf("\n === Writing compressed output completed ==\n");
   
    free(buffer);
    if (fclose(file) != 0) {
        perror("Warning: Error closing output file");
    }
    return body_size+header_size;
}
// === FILE: /home/noman/takatuka/takatuka_compression/src/files/compression/compressed_body.c ===

// compressed_body.c
//
// Writes the compressed body using the code_map filled by populate_header().
// Uses global_class2_bits (written in header) to determine class-2 code widths.

#include "compressed_body.h"
#include "bit_writer.h"
#include "code_classes.h"
#include "code_map.h"
#include "compressed_header.h" // for global_class2_bits and global_rle_bits
#include "graph.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

extern CodeMap code_map; // filled by populate_header()
extern uint8_t global_class2_bits; // from compressed_header.h  
extern uint8_t global_rle_bits;    // from compressed_header.h

long populate_body(BestPathView best_path, const uint8_t *block, FILE *file_to_write, BitWriter *writer) {
#ifdef DEBUG
    printf("\n\n\n******************************* \n\n[DEBUG] Starting body population with path size: %d\n",
           best_path.path_size);
    printf("[DEBUG] Initial BitWriter state:\n\n\n");
    bitwriter_print_state(writer);
#endif

    for (int32_t i = best_path.path_size - 1; i >= 0; --i) {
        GraphNode *node = get_graph_node(best_path.nodes[i]);
        if (!node || node->node_id == 0) {
#ifdef DEBUG
            printf("[DEBUG] Skipping node at position %d (null or root)\n", i);
#endif
            continue;
        }

        const uint8_t *seq = &block[node->offset];
        uint8_t len = node->sequence_length;

#ifdef DEBUG
        printf("\n\n[DEBUG] Processing node %d: offset=%u, len=%u, RLE=%d\n", node->node_id, node->offset, len,
               node->RLE_type);
#endif
        uint16_t code;
        uint8_t code_class;
        if (node->RLE_type) {
            // RLE case: encoded using code_class = 3 (bits '11') and RLE format
#ifdef DEBUG
            printf("[DEBUG] Writing RLE sequence (count=%u, using %u bits)\n", node->length_of_RLE, global_rle_bits);
#endif

            // Write compressed flag (1) then class (3 == 11b)
            SAFE_BITWRITE(writer, 1, 1, file_to_write, "c_flag");
#ifdef DEBUG
            printf("[DEBUG] ➤ Written 1 bit: 1 (compressed flag)\n");
            bitwriter_print_state(writer);
#endif
            SAFE_BITWRITE(writer, 3, 2, file_to_write, "code_class"); // RLE uses class code 11b
#ifdef DEBUG
            printf("[DEBUG] ➤ Written 2 bits: code class = %u\n", 3);
            bitwriter_print_state(writer);
#endif

            // Use dynamic RLE bits instead of fixed 8 bits
            if (global_rle_bits > 0) {
                SAFE_BITWRITE(writer, node->length_of_RLE, global_rle_bits, file_to_write, "len_of_RLE");
#ifdef DEBUG
                printf("[DEBUG] ➤ Written %u bits: RLE count = %u\n", global_rle_bits, node->length_of_RLE);
                bitwriter_print_state(writer);
#endif
            } else {
                // Fallback: should only happen if there are no RLE sequences
                SAFE_BITWRITE(writer, node->length_of_RLE, 8, file_to_write, "len_of_RLE");
#ifdef DEBUG
                printf("[DEBUG] ➤ Written 8 bits (fallback): RLE count = %u\n", node->length_of_RLE);
                bitwriter_print_state(writer);
#endif
            }

            // Write the RLE pattern byte (always 8 bits)
            uint8_t j = 0;
            SAFE_BITWRITE(writer, seq[j], 8, file_to_write, "");
#ifdef DEBUG
            printf("[DEBUG] ➤ Written 8 bits: RLE pattern byte %02X\n", seq[j]);
            bitwriter_print_state(writer);
#endif
        } else if (len > 1 && code_map_get(&code_map, seq, len, &code, &code_class)) {
            // Compressed regular sequence - write compressed flag, class, and code index.
#ifdef DEBUG
            printf("[DEBUG] Writing compressed sequence (code=%u, class=%u)\n", code, code_class);
#endif

            SAFE_BITWRITE(writer, 1, 1, file_to_write, "c_flag");
#ifdef DEBUG
            printf("[DEBUG] ➤ Written 1 bit: 1 (compressed flag)\n");
            bitwriter_print_state(writer);
#endif

            SAFE_BITWRITE(writer, code_class, 2, file_to_write, "code_class");
#ifdef DEBUG
            printf("[DEBUG] ➤ Written 2 bits: code class = %u\n", code_class);
            bitwriter_print_state(writer);
#endif

            // Use class2 bit-width if code_class == 2, else fixed sizes from code_classes
            uint8_t bits_for_code = get_code_class_size(code_class, global_class2_bits);
            SAFE_BITWRITE(writer, code, bits_for_code, file_to_write, "code");
#ifdef DEBUG
            printf("[DEBUG] ➤ Written %u bits: code = %u\n", bits_for_code, code);
            bitwriter_print_state(writer);
#endif
        } else {
            // Uncompressed bytes: each raw byte prefixed with a '0' flag bit.
#ifdef DEBUG
            printf("[DEBUG] Writing uncompressed bytes (len=%u)\n", len);
#endif
            for (uint8_t j = 0; j < len; ++j) {
                SAFE_BITWRITE(writer, 0, 1, file_to_write, "seq_len");
#ifdef DEBUG
                printf("[DEBUG] ➤ Written 1 bit: 0 (uncompressed flag)\n");
                bitwriter_print_state(writer);
#endif
                SAFE_BITWRITE(writer, seq[j], 8, file_to_write, "");
#ifdef DEBUG
                printf("[DEBUG] ➤ Written 8 bits: raw byte %02X\n", seq[j]);
                bitwriter_print_state(writer);
#endif
            }
        }
    }

#ifdef DEBUG
    printf("[DEBUG] Body writing complete, flushing...\n");
#endif
    // Record file offset before flush
    long body_start = ftell(file_to_write);

    if (!bitwriter_write_to_file(writer, file_to_write)) {
        fprintf(stderr, "Failed to write final body data to file\n");
        exit(EXIT_FAILURE);
    }
    // Record file offset after flush
    long body_end = ftell(file_to_write);

    printf("Body size written: %ld bytes\n", body_end - body_start);

#ifdef DEBUG
    printf("[DEBUG] Body written successfully\n");
    bitwriter_print_state(writer);
#endif

    return body_end - body_start;
}
// === FILE: /home/noman/takatuka/takatuka_compression/src/files/compression/compressed_body.h ===

// compressed_body.h

#pragma once

#include "best_path_view.h"
#include "bit_writer.h"
#include "compressed_header.h"
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

extern uint8_t global_rle_bits;  // Declare external variable from compressed_header.h

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
long populate_body(BestPathView best_path, const uint8_t *block, FILE *file_to_write, BitWriter *writer);

// === FILE: /home/noman/takatuka/takatuka_compression/src/files/compression/compressed_header.c ===

// compressed_header.c
//
// Updated header writer to emit the compact format:
//  - 16 bits : total number of codewords (placeholder then overwritten)
//  - 8 bits  : class2_bits
//  - 8 bits  : len_bits[0]
//  - 8 bits  : len_bits[1]
//  - 8 bits  : len_bits[2]
// Followed by, for class = 0..2 and for each entry in class index order:
//  - if len_bits[class] > 0: write (length - 1) in len_bits[class] bits
//  - write (length) bytes: the raw sequence
//
// This file keeps your original candidate selection, sorting and greedy class
// assignment behavior; only the header emission is changed.

#include "compressed_header.h"
#include "bit_writer.h"
#include "code_classes.h"
#include "code_map.h"
#include "compressed_body.h"
#include "graph.h"
#include "seq_freq_map.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEADER_BUFFER_SIZE 4096

/* exported globals declared in compressed_header.h */
uint8_t global_rle_bits;
uint8_t global_class2_bits = 0;
CodeMap code_map;

typedef struct {
    uint64_t savings;
    const uint8_t *sequence; // pointer into block (no copy)
    uint8_t length;
} CodeCandidate;

/* Compare descending by savings so highest savings are encoded first */
static int compare_candidates_desc(const void *a, const void *b) {
    const CodeCandidate *ca = (const CodeCandidate *)a;
    const CodeCandidate *cb = (const CodeCandidate *)b;
    return (cb->savings > ca->savings) - (cb->savings < ca->savings);
}

/* Helper: whether a node should be skipped for header code creation */
static inline bool should_skip_node(const GraphNode *node, uint32_t freq) {
    return (node->sequence_length == 1) || (freq == 1) || (node->RLE_type) || (node->node_id == 0);
}

/* Check whether the sequence at block[offset] has already been seen; if not, mark it with freq2 */
static inline uint8_t sequence_seen(const uint8_t *block, uint8_t length, uint32_t freq2) {
    uint32_t freq, node_id;
    seq_freq_get(block, length, &freq, &node_id);
    if (freq >= 1) return 1;
    seq_freq_set(block, length, freq2, 0); // set it for future use.
    return 0;
}

/* Helper: compute ceil_log2 for positive integer x (returns 0 for x<=1) */
static uint8_t ceil_log2_u32(uint32_t x) {
    if (x <= 1) return 0;
    uint8_t n = 0;
    uint32_t v = 1u;
    while (v < x) {
        v <<= 1;
        n++;
    }
    return n;
}

static inline uint8_t calculate_bit_length(uint16_t value) {
    if (value == 0) return 1; // At least 1 bit needed to represent 0
    uint8_t bits = 0;
    while (value > 0) {
        bits++;
        value >>= 1;
    }
    return bits;
}

long populate_header(BestPathView best_path, const uint8_t *block, FILE *file_to_write, BitWriter *writer) {
    init_seq_freq_map();

#ifdef DEBUG
    printf("[DEBUG] Initialized sequence frequency map with capacity: %u\n", best_path.path_size * 2);
    printf("[DEBUG] Initial BitWriter state:\n");
    bitwriter_print_state(writer);
#endif

    // Reserve first two bytes (16 bits) for number of codes (we will overwrite later).
    SAFE_BITWRITE(writer, 0, 16, file_to_write, "header_length"); // placeholder
    size_t header_start_bit = writer->byte_pos * 8 + writer->bit_pos - 16;

#ifdef DEBUG
    printf("[DEBUG] Reserved 16 bits for code count at bit position %zu\n", header_start_bit);
    bitwriter_print_state(writer);
#endif

    // Collect candidates
    CodeCandidate *candidates = malloc(sizeof(CodeCandidate) * best_path.path_size);
    if (!candidates) {
        fprintf(stderr, "Failed to allocate candidates array\n");
        exit(EXIT_FAILURE);
    }
    int candidate_count = 0;
    uint16_t max_rle_count = 0;
    for (int32_t i = best_path.path_size - 1; i >= 0; --i) {
        GraphNode *node = get_graph_node(best_path.nodes[i]);
        if (!node) continue;
        // Track maximum RLE count
        if (node->RLE_type && node->length_of_RLE > max_rle_count) {
            max_rle_count = node->length_of_RLE;
        }
        uint32_t freq = best_path.freqs[i];
        uint8_t len = node->sequence_length;
        if (should_skip_node(node, freq)) continue;
        uint32_t offset = node->offset;
        if (sequence_seen(&block[offset], len, freq)) continue;
        const uint8_t *sequence = &block[offset];
        candidates[candidate_count++] = (CodeCandidate){.savings = (uint64_t)freq, .sequence = sequence, .length = len};
    }
    // Calculate RLE bits using the helper function
    if (max_rle_count > 0) {
        global_rle_bits = calculate_bit_length(max_rle_count);
        // Cap at 8 bits since RLE count is uint8_t
        if (global_rle_bits > 8) global_rle_bits = 8;
    } else {
        global_rle_bits = 0; // No RLE sequences
    }

    printf("Maximum RLE count: %u, using %u bits for RLE encoding\n", max_rle_count, global_rle_bits);

    if (candidate_count == 0) {
#ifdef DEBUG
        printf("[DEBUG] No candidates found for header\n");
#endif
        if (!bitwriter_write_to_file(writer, file_to_write)) {
            fprintf(stderr, "Failed to write empty header\n");
            exit(EXIT_FAILURE);
        }
        bitwriter_reset(writer);
        free(candidates);
        global_class2_bits = 0; // nothing to do in body
        return 0;
    }

    // Sort and init code_map
    qsort(candidates, candidate_count, sizeof(CodeCandidate), compare_candidates_desc);
    init_code_map(&code_map, candidate_count);

#ifdef DEBUG
    printf("[DEBUG] Sorted %d candidates, initialized code map\n", candidate_count);
#endif

    // Fixed capacities class0 and class1
    uint32_t max_class0 = get_code_class_threshold(0, 0);
    uint32_t max_class1 = get_code_class_threshold(1, 0);

    // Plan greedy fill counts (without writing yet)
    uint32_t will_fill_class0 = (candidate_count <= (int)max_class0) ? (uint32_t)candidate_count : max_class0;
    uint32_t remaining_after_class0 = (uint32_t)candidate_count - will_fill_class0;
    uint32_t will_fill_class1 = (remaining_after_class0 <= max_class1) ? remaining_after_class0 : max_class1;
    uint32_t will_fill_class2 = (uint32_t)candidate_count - will_fill_class0 - will_fill_class1;

#ifdef DEBUG
    printf("[DEBUG] planned fill: class0=%u, class1=%u, class2=%u\n", will_fill_class0, will_fill_class1,
           will_fill_class2);
#endif

    // Determine class2 bits and capacities
    uint8_t class2_bits = calculate_class2_bits((uint16_t)will_fill_class2);
    global_class2_bits = class2_bits;

    printf("Total number of codes=%d, Computed class2_bits = %u\n", candidate_count, class2_bits);

    uint32_t max_class2 = get_code_class_threshold(2, class2_bits);

    printf("Capacities after class2_bits: class0=%u, class1=%u, class2=%u\n", max_class0, max_class1, max_class2);

    // Simulate assignment to determine per-class ordering and index-within-class
    uint8_t *assigned_class = malloc((size_t)candidate_count);
    uint16_t *index_within_class = malloc(sizeof(uint16_t) * (size_t)candidate_count);
    uint16_t assigned_count[3] = {0, 0, 0};
    uint32_t max_per_class[3] = {max_class0, max_class1, max_class2};

    if (!assigned_class || !index_within_class) {
        fprintf(stderr, "OOM simulating assignment\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < candidate_count; ++i) {
        int cls = -1;
        for (int c = 0; c <= 2; ++c) {
            if (assigned_count[c] < max_per_class[c]) {
                cls = c;
                break;
            }
        }
        if (cls == -1) {
            fprintf(stderr, "Error: All code classes full during simulation at i=%d\n", i);
            free(candidates);
            exit(EXIT_FAILURE);
        }
        assigned_class[i] = (uint8_t)cls;
        index_within_class[i] = assigned_count[cls];
        assigned_count[cls]++;
    }

    // Compute maximum length per class
    uint32_t max_len[3] = {0, 0, 0};
    for (int i = 0; i < candidate_count; ++i) {
        int cls = assigned_class[i];
        if (candidates[i].length > max_len[cls]) max_len[cls] = candidates[i].length;
    }

    // Determine len_bits per class:
    // len_bits[c] = minimum bits to represent (length_minus_one) in [0 .. max_len[c]-1]
    // If max_len[c] <= 1, len_bits[c] == 0 (no bits stored; decoder must treat length=1)
    uint8_t len_bits[3];
    for (int c = 0; c < 3; ++c) {
        len_bits[c] = ceil_log2_u32((uint32_t)(max_len[c] > 0 ? max_len[c] : 1));
    }

#ifdef DEBUG
    printf("[DEBUG] len_bits per class: [%u, %u, %u]\n", len_bits[0], len_bits[1], len_bits[2]);
#endif

    // Write class2_bits and len_bits (each as one byte)
    SAFE_BITWRITE(writer, class2_bits, 8, file_to_write, "class2_bits");
    SAFE_BITWRITE(writer, len_bits[0], 8, file_to_write, "len_bits0");
    SAFE_BITWRITE(writer, len_bits[1], 8, file_to_write, "len_bits1");
    SAFE_BITWRITE(writer, len_bits[2], 8, file_to_write, "len_bits2");

    // === ADD THIS RIGHT HERE ===
    SAFE_BITWRITE(writer, global_rle_bits, 3, file_to_write, "rle_bits");
    // ===========================

#ifdef DEBUG
    printf("[DEBUG] Wrote class2_bits and len_bits header fields\n");
    bitwriter_print_state(writer);
#endif

    // Build per-class index -> candidate index mapping so we can write in class order
    uint16_t *per_class_offsets[3] = {NULL, NULL, NULL};
    for (int c = 0; c < 3; ++c) {
        if (assigned_count[c] > 0) {
            per_class_offsets[c] = malloc(sizeof(uint16_t) * assigned_count[c]);
            if (!per_class_offsets[c]) {
                fprintf(stderr, "OOM allocating per_class_offsets\n");
                exit(EXIT_FAILURE);
            }
            for (uint16_t k = 0; k < assigned_count[c]; ++k)
                per_class_offsets[c][k] = UINT16_MAX;
        }
    }

    for (int i = 0; i < candidate_count; ++i) {
        int cls = assigned_class[i];
        uint16_t idx = index_within_class[i];
        per_class_offsets[cls][idx] = (uint16_t)i;
    }

    // Emit entries in class order (and populate code_map with index-within-class)
    for (int cls = 0; cls <= 2; ++cls) {
        uint16_t cnt = assigned_count[cls];
        uint8_t lb = len_bits[cls];
        for (uint16_t idx = 0; idx < cnt; ++idx) {
            uint16_t cand_i = per_class_offsets[cls][idx];
            CodeCandidate *cand = &candidates[cand_i];

            // Register into code_map: index within class = idx
            code_map_set(&code_map, cand->sequence, cand->length, idx, (uint8_t)cls);

            uint32_t length = cand->length;
            if (lb > 0) {
                uint32_t value = length - 1; // encode length_minus_one
                SAFE_BITWRITE(writer, value, lb, file_to_write, "seq_len_compact");
            }
            // Write sequence bytes
            for (uint32_t j = 0; j < length; ++j) {
                SAFE_BITWRITE(writer, cand->sequence[j], 8, file_to_write, "seq_byte");
            }
        }
    }

#ifdef DEBUG
    printf("[DEBUG] Final code map state:\n");
    print_code_map(&code_map);
    printf("[DEBUG] Header writing complete, flushing...\n");
#endif

    // Overwrite placeholder with actual count
    bitwriter_overwrite_at(writer, header_start_bit, candidate_count, 16);

    // Flush to file
    long header_start = ftell(file_to_write);
    if (!bitwriter_write_to_file(writer, file_to_write)) {
        fprintf(stderr, "Failed to write header to file\n");
        exit(EXIT_FAILURE);
    }
    long header_end = ftell(file_to_write);

    printf("Header size written: %ld bytes\n", header_end - header_start);

    // Reset writer positions and cleanup
    bitwriter_reset_positions(writer);

    free(candidates);
    free(assigned_class);
    free(index_within_class);
    for (int c = 0; c < 3; ++c) {
        if (per_class_offsets[c]) free(per_class_offsets[c]);
    }

    return header_end - header_start;
}

// === FILE: /home/noman/takatuka/takatuka_compression/src/files/compression/compressed_header.h ===

#pragma once

#pragma once

#include "best_path_view.h"
#include "code_map.h"
#include "seq_freq_map.h"
#include "bit_writer.h"
#include <stdint.h>
#include <string.h>

//rle count length. It is computed dynamically.
extern uint8_t global_rle_bits;  

extern CodeMap code_map;
/*
 * Exposed symbol so compressed_body.c can use same class2 bit-width
 * as written into the header by populate_header().
 *
 * This value is written into the compressed file as a single byte
 * and should be read by any decoder prior to decoding body.
 *
 *  - 0 means "no class2 codes present"
 *  - otherwise value is number of bits used for class2 indices
 */
extern uint8_t global_class2_bits;

// Function to populate header
long populate_header(BestPathView best_path, const uint8_t *block, FILE *file_to_write, BitWriter *writer);


// === FILE: /home/noman/takatuka/takatuka_compression/src/files/compression/compress.h ===

#pragma once

long write_compressed_output(const char* filename, const uint8_t* block);
                          
                          
