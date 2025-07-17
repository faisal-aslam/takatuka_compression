// Combined C and H Files

// === FILE: /home/noman/takatuka/takatuka_compression/src/files/compression/bit_writer.c ===

// bit_writer.c
#include "bit_writer.h"
#include <string.h>

void bitwriter_init(BitWriter* bw, uint8_t* buffer, size_t size) {
    bw->buffer = buffer;
    bw->buffer_size = size;
    bw->byte_pos = 0;
    bw->bit_pos = 0;
    bw->overflow = false;
    memset(buffer, 0, size);
}

bool bitwriter_write(BitWriter* bw, uint32_t value, uint8_t num_bits) {
    if (num_bits > 32 || bw->overflow) return false;

    for (int i = num_bits - 1; i >= 0; --i) {
        if (bw->byte_pos >= bw->buffer_size) {
            bw->overflow = true;
            return false;
        }

        uint8_t bit = (value >> i) & 1;
        bw->buffer[bw->byte_pos] |= bit << (7 - bw->bit_pos);
        bw->bit_pos++;

        if (bw->bit_pos == 8) {
            bw->bit_pos = 0;
            bw->byte_pos++;
        }
    }
    return true;
}

void bitwriter_flush(BitWriter* bw) {
    if (bw->bit_pos != 0) {
        bw->byte_pos++;
        bw->bit_pos = 0;
    }
}

size_t bitwriter_bytes_written(const BitWriter* bw) {
    return bw->byte_pos + (bw->bit_pos != 0 ? 1 : 0);
}

bool bitwriter_write_to_file(const BitWriter* bw, FILE* fp) {
    size_t bytes_to_write = bitwriter_bytes_written(bw);
    return fwrite(bw->buffer, 1, bytes_to_write, fp) == bytes_to_write;
}

void bitwriter_print_state(const BitWriter* bw) {
    printf("[BitWriter] byte_pos = %zu, bit_pos = %u, total_bits = %zu, overflow = %s\n",
           bw->byte_pos, bw->bit_pos,
           bw->byte_pos * 8 + bw->bit_pos,
           bw->overflow ? "true" : "false");
}


bool bitwriter_overwrite_at(BitWriter* bw, size_t bit_pos, uint32_t value, uint8_t num_bits) {
    if (num_bits > 32 || bw->overflow) return false;

    size_t max_bits = bw->buffer_size * 8;
    if (bit_pos + num_bits > max_bits) {
        bw->overflow = true;
        return false;
    }

    for (int i = num_bits - 1; i >= 0; --i) {
        size_t current_bit = bit_pos + (num_bits - 1 - i);
        size_t byte_index = current_bit / 8;
        size_t bit_index = 7 - (current_bit % 8);  // MSB-first

        uint8_t bit = (value >> i) & 1;
        if (bit)
            bw->buffer[byte_index] |= (1 << bit_index);
        else
            bw->buffer[byte_index] &= ~(1 << bit_index);
    }

    return true;
}


void bitwriter_reset(BitWriter* bw) {
    bw->byte_pos = 0;
    bw->bit_pos = 0;
    bw->overflow = false;
    memset(bw->buffer, 0, bw->buffer_size);
}

// === FILE: /home/noman/takatuka/takatuka_compression/src/files/compression/bit_writer.h ===

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct {
    uint8_t* buffer;
    size_t buffer_size;
    size_t byte_pos;
    uint8_t bit_pos;  // 0 (MSB) to 7 (LSB)
    bool overflow;
} BitWriter;

void bitwriter_init(BitWriter* bw, uint8_t* buffer, size_t size);
bool bitwriter_write(BitWriter* bw, uint32_t value, uint8_t num_bits);
void bitwriter_flush(BitWriter* bw);
size_t bitwriter_bytes_written(const BitWriter* bw);
bool bitwriter_write_to_file(const BitWriter* bw, FILE* fp);
void bitwriter_print_state(const BitWriter* bw);
bool bitwriter_overwrite_at(BitWriter* bw, size_t bit_pos, uint32_t value, uint8_t num_bits);
void bitwriter_reset(BitWriter* bw);


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
                printf("%02X ", e->seq[j]);
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
void write_compressed_output(const char* filename, const uint8_t* block) {
    if (!filename || !block) {
        fprintf(stderr, "Error: Invalid inputs in writeCompressedOutput\n");
        return;
    }

    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open output file");
        return;
    }

    BestPathView best_view = get_best_path_view();
#ifdef DEBUG
    printf("[DEBUG] Initial best path view:\n");
    print_best_view(&best_view, 1, block);
#endif

    printf("\n ==== Starting compressed output writing === \n");

    // Allocate a single buffer for both header and body
    size_t buffer_size = BUFFER_SIZE;
    uint8_t* buffer = malloc(buffer_size);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate buffer\n");
        fclose(file);
        return;
    }

    BitWriter writer;
    bitwriter_init(&writer, buffer, buffer_size);

#ifdef DEBUG
    printf("[DEBUG] Initialized BitWriter with buffer size: %zu\n", buffer_size);
#endif

    // Write header
    populate_header(best_view, block, file, &writer);

#ifdef DEBUG
    printf("[DEBUG] After header writing:\n");
    print_best_view(&best_view, 1, block);
    printf("[DEBUG] BitWriter state after header:\n");
    bitwriter_print_state(&writer);
#endif

    // Write body
    populate_body(best_view, block, file, &writer);

#ifdef DEBUG
    printf("[DEBUG] BitWriter state after body:\n");
    bitwriter_print_state(&writer);
#endif

    printf("\n === Writing compressed output completed ==\n");
   
    free(buffer);
    if (fclose(file) != 0) {
        perror("Warning: Error closing output file");
    }
}
// === FILE: /home/noman/takatuka/takatuka_compression/src/files/compression/compressed_body.c ===

//compressed_body.c

#include "compressed_body.h"
#include "compressed_header.h"
#include "bit_writer.h"
#include "code_map.h"
#include "code_classes.h"
#include "graph.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>


extern CodeMap code_map;

void populate_body(BestPathView best_path, const uint8_t* block, 
                  FILE* file_to_write, BitWriter* writer) {
#ifdef DEBUG
    printf("[DEBUG] Starting body population with path size: %d\n", best_path.path_size);
    printf("[DEBUG] Initial BitWriter state:\n");
    bitwriter_print_state(writer);
#endif

    for (int32_t i = best_path.path_size - 1; i >= 0; --i) {
        GraphNode* node = get_graph_node(best_path.nodes[i]);
        if (!node || node->node_id == 0) {
#ifdef DEBUG
            printf("[DEBUG] Skipping node at position %d (null or root)\n", i);
#endif
            continue;
        }

        const uint8_t* seq = &block[node->offset];
        uint8_t len = node->sequence_length;
        uint32_t freq = best_path.freqs[i];

#ifdef DEBUG
        printf("[DEBUG] Processing node %d: offset=%u, len=%u, freq=%u, RLE=%d\n",
               node->node_id, node->offset, len, freq, node->is_RLE);
#endif

        if (len == 1 || freq == 1) {
            // Uncompressed case
#ifdef DEBUG
            printf("[DEBUG] Writing uncompressed bytes (len=%u)\n", len);
#endif
            for (uint8_t j = 0; j < len; ++j) {
                SAFE_BITWRITE(writer, 0, 1, file_to_write);
#ifdef DEBUG
                printf("[DEBUG] ➤ Written 1 bit: 0 (uncompressed flag)\n");
                bitwriter_print_state(writer);
#endif
                SAFE_BITWRITE(writer, seq[j], 8, file_to_write);
#ifdef DEBUG
                printf("[DEBUG] ➤ Written 8 bits: raw byte %02X\n", seq[j]);
                bitwriter_print_state(writer);
#endif
            }
        }
        else if (node->is_RLE) {
            // RLE case
            if (node->repeat_seq_length > 8) {
                fprintf(stderr, "RLE repeat_seq_length too large: %u\n", node->repeat_seq_length);
                exit(EXIT_FAILURE);
            }

#ifdef DEBUG
            printf("[DEBUG] Writing RLE sequence (repeat_len=%u, count=%u)\n",
                   node->repeat_seq_length, node->length_of_RLE);
#endif

            SAFE_BITWRITE(writer, 1, 1, file_to_write);
#ifdef DEBUG
            printf("[DEBUG] ➤ Written 1 bit: 1 (compressed flag)\n");
            bitwriter_print_state(writer);
#endif
            SAFE_BITWRITE(writer, node->repeat_seq_length, 3, file_to_write);
#ifdef DEBUG
            printf("[DEBUG] ➤ Written 3 bits: RLE repeat length = %u\n", node->repeat_seq_length);
            bitwriter_print_state(writer);
#endif
            SAFE_BITWRITE(writer, node->length_of_RLE, 8, file_to_write);
#ifdef DEBUG
            printf("[DEBUG] ➤ Written 8 bits: RLE count = %u\n", node->length_of_RLE);
            bitwriter_print_state(writer);
#endif

            for (uint8_t j = 0; j < node->repeat_seq_length; ++j) {
                SAFE_BITWRITE(writer, seq[j], 8, file_to_write);
#ifdef DEBUG
                printf("[DEBUG] ➤ Written 8 bits: RLE pattern byte %02X\n", seq[j]);
                bitwriter_print_state(writer);
#endif
            }
        }
        else {
            // Compressed non-RLE
            uint16_t code;
            uint8_t code_class;
            if (!code_map_get(&code_map, seq, len, &code, &code_class)) {
                fprintf(stderr, "Missing code_map entry for compressed sequence\n");
                exit(EXIT_FAILURE);
            }

#ifdef DEBUG
            printf("[DEBUG] Writing compressed sequence (code=%u, class=%u)\n", code, code_class);
#endif

            SAFE_BITWRITE(writer, 1, 1, file_to_write);
#ifdef DEBUG
            printf("[DEBUG] ➤ Written 1 bit: 1 (compressed flag)\n");
            bitwriter_print_state(writer);
#endif
            SAFE_BITWRITE(writer, code_class, 2, file_to_write);
#ifdef DEBUG
            printf("[DEBUG] ➤ Written 2 bits: code class = %u\n", code_class);
            bitwriter_print_state(writer);
#endif
            SAFE_BITWRITE(writer, code, get_code_class_size(code_class), file_to_write);
#ifdef DEBUG
            printf("[DEBUG] ➤ Written %u bits: code = %u\n", 
                   get_code_class_size(code_class), code);
            bitwriter_print_state(writer);
#endif
        }
    }

#ifdef DEBUG
    printf("[DEBUG] Body writing complete, flushing...\n");
#endif

    bitwriter_flush(writer);
    if (!bitwriter_write_to_file(writer, file_to_write)) {
        fprintf(stderr, "Failed to write final body data to file\n");
        exit(EXIT_FAILURE);
    }

#ifdef DEBUG
    printf("[DEBUG] Body written successfully\n");
    bitwriter_print_state(writer);
#endif
}
// === FILE: /home/noman/takatuka/takatuka_compression/src/files/compression/compressed_body.h ===

// compressed_body.h

#pragma once

#include <stdint.h>
#include <stdio.h>  
#include <string.h>
#include "best_path_view.h"
#include "bit_writer.h"

// Unified safe writing macro for both header and body
#define SAFE_BITWRITE(bw, value, bits, file)                   \
    do {                                                        \
        if (!bitwriter_write(bw, value, bits)) {                \
            bitwriter_flush(bw);                               \
            if (!bitwriter_write_to_file(bw, file)) {           \
                fprintf(stderr, "Failed to write buffer to file\n"); \
                exit(EXIT_FAILURE);                            \
            }                                                  \
            bitwriter_reset(bw);                               \
            if (!bitwriter_write(bw, value, bits)) {            \
                fprintf(stderr, "bitwriter_write failed after flush\n"); \
                exit(EXIT_FAILURE);                            \
            }                                                   \
        }                                                       \
    } while (0)

/**
 * @brief Processes BestPathView and writes compressed data to output file
 * 
 * Full processing logic:
 * 1. Reverse traversal of BestPathView
 * 2. Handles uncompressed nodes (prefix 0 + raw byte for each byte)
 * 3. Handles compressed non-RLE nodes (prefix 1 + code class + code)
 * 4. Handles RLE nodes (prefix 1 + repeat len + RLE count + sequence)
 * 
 * 
 * @param best_path The optimal compression path
 * @param block Input data block
 * @param file_to_write Output file handle
 * @param writer BitWriter instance to use
 */
void populate_body(BestPathView best_path, const uint8_t* block, 
                  FILE* file_to_write, BitWriter* writer);

// === FILE: /home/noman/takatuka/takatuka_compression/src/files/compression/compressed_header.c ===

//compressed_header.c

#include "compressed_header.h"
#include "bit_writer.h"
#include "code_classes.h"
#include "../graph/graph.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "seq_freq_map.h"
#include "code_map.h"

#define HEADER_BUFFER_SIZE 4096
static SeqFreqMap seq_map;

CodeMap code_map;
 
static inline bool should_skip_node(const GraphNode* node, uint32_t freq) {
    return (node->sequence_length == 1) || 
           (freq == 1) || 
           (node->is_RLE) || 
           (node->node_id == 0);
}

static inline uint8_t sequence_seen(const uint8_t *block, uint8_t length, uint32_t freq) {
    if (seq_freq_get(&seq_map, block, length, -1) >= 1) return 1;
    seq_freq_set(&seq_map, block, length, freq); //set it for future use.
    return 0;
}

typedef struct {
    uint64_t savings;
    const uint8_t* sequence;  // pointer into block (no copy)
    uint8_t length;
} CodeCandidate;


int compare_candidates_desc(const void* a, const void* b) {
    const CodeCandidate* ca = (const CodeCandidate*)a;
    const CodeCandidate* cb = (const CodeCandidate*)b;
    return (cb->savings > ca->savings) - (cb->savings < ca->savings);
}

void populate_header(BestPathView best_path, const uint8_t* block, 
                    FILE* file_to_write, BitWriter* writer) {
    init_seq_freq_map(&seq_map, best_path.path_size * 2);

#ifdef DEBUG
    printf("[DEBUG] Initialized sequence frequency map with capacity: %zu\n", best_path.path_size * 2);
    printf("[DEBUG] Initial BitWriter state:\n");
    bitwriter_print_state(writer);
#endif

    // Reserve first two bytes for number of codes
    SAFE_BITWRITE(writer, 0, 16, file_to_write); // placeholder
    size_t header_start_bit = writer->byte_pos * 8 + writer->bit_pos - 16;

#ifdef DEBUG
    printf("[DEBUG] Reserved 16 bits for code count at bit position %zu\n", header_start_bit);
    bitwriter_print_state(writer);
#endif

    uint16_t assigned[3] = {0};
    uint32_t max_per_class[3] = {
        get_code_class_threshold(0),
        get_code_class_threshold(1),
        get_code_class_threshold(2)
    };

#ifdef DEBUG
    printf("[DEBUG] Code class thresholds: Class0=%u, Class1=%u, Class2=%u\n",
           max_per_class[0], max_per_class[1], max_per_class[2]);
#endif

    // Step 1: Collect unique sequences with savings
    CodeCandidate* candidates = malloc(sizeof(CodeCandidate) * best_path.path_size);
    if (!candidates) {
        fprintf(stderr, "Failed to allocate candidates array\n");
        exit(EXIT_FAILURE);
    }
    int candidate_count = 0;

    for (int32_t i = best_path.path_size - 1; i >= 0; --i) {
        GraphNode *node = get_graph_node(best_path.nodes[i]);
        if (!node) {
#ifdef DEBUG
            printf("[DEBUG] Node at position %d is null\n", i);
#endif
            continue;
        }

        uint32_t freq = best_path.freqs[i];
        uint8_t len = node->sequence_length;

        if (should_skip_node(node, freq)) {
#ifdef DEBUG
            printf("[DEBUG] Skipping node %d (len=%u, freq=%u, RLE=%d)\n", 
                   node->node_id, len, freq, node->is_RLE);
#endif
            continue;
        }

        uint32_t offset = node->offset;
        if (sequence_seen(&block[offset], len, freq)) {
#ifdef DEBUG
            printf("[DEBUG] Sequence already seen at offset %u (len=%u)\n", offset, len);
#endif
            continue;
        }

        const uint8_t *sequence = &block[offset];
        candidates[candidate_count++] = (CodeCandidate){
            .savings = (uint64_t)len * freq,
            .sequence = sequence,
            .length = len
        };

#ifdef DEBUG
        printf("[DEBUG] Added candidate #%d: len=%u, savings=%lu, offset=%u\n", 
               candidate_count, len, (uint64_t)len * freq, offset);
#endif
    }

    if (candidate_count == 0) {
#ifdef DEBUG
        printf("[DEBUG] No candidates found for header\n");
#endif
        bitwriter_flush(writer);
        if (!bitwriter_write_to_file(writer, file_to_write)) {
            fprintf(stderr, "Failed to write empty header\n");
            exit(EXIT_FAILURE);
        }
        bitwriter_reset(writer);
        free_seq_freq_map(&seq_map);
        free(candidates);
        return;
    }

    // Step 2: Sort descending by savings
    qsort(candidates, candidate_count, sizeof(CodeCandidate), compare_candidates_desc);
    init_code_map(&code_map, candidate_count);

#ifdef DEBUG
    printf("[DEBUG] Sorted %d candidates, initialized code map\n", candidate_count);
    printf("[DEBUG] Candidate savings order:\n");
    for (int i = 0; i < candidate_count && i < 10; i++) {
        printf("  #%d: savings=%lu, len=%u\n", i, candidates[i].savings, candidates[i].length);
    }
    if (candidate_count > 10) {
        printf("  ... and %d more\n", candidate_count - 10);
    }
#endif

    // Step 3: Encode in savings order
    for (int i = 0; i < candidate_count; ++i) {
        CodeCandidate* cand = &candidates[i];
        
        // Find lowest available class
        int code_class = -1;
        for (int c = 0; c <= 2; ++c) {
            if (assigned[c] < max_per_class[c]) {
                code_class = c;
                break;
            }
        }

        if (code_class == -1) {
            fprintf(stderr, "Error: All code classes full after encoding %d sequences\n", i);
            free(candidates);
            exit(EXIT_FAILURE);
        }

        code_map_set(&code_map, cand->sequence, cand->length, assigned[code_class], code_class);

#ifdef DEBUG
        printf("[DEBUG] Encoding candidate #%d (class=%d, code=%u, len=%u): ", 
               i, code_class, assigned[code_class], cand->length);
        for (uint8_t j = 0; j < cand->length && j < 8; ++j) {
            printf("%02X ", cand->sequence[j]);
        }
        if (cand->length > 8) printf("...");
        printf("\n");
#endif

        SAFE_BITWRITE(writer, code_class, 2, file_to_write);
#ifdef DEBUG
        printf("[DEBUG] ➤ Written 2 bits: code_class = %u\n", code_class);
        bitwriter_print_state(writer);
#endif

        SAFE_BITWRITE(writer, cand->length, 8, file_to_write);
#ifdef DEBUG
        printf("[DEBUG] ➤ Written 8 bits: length = %u\n", cand->length);
        bitwriter_print_state(writer);
#endif

        uint8_t code_bits = get_code_class_size(code_class);
        SAFE_BITWRITE(writer, assigned[code_class], code_bits, file_to_write);
#ifdef DEBUG
        printf("[DEBUG] ➤ Written %u bits: code index in class[%u] = %u\n", 
               code_bits, code_class, assigned[code_class]);
        bitwriter_print_state(writer);
#endif

        for (uint8_t j = 0; j < cand->length; ++j) {
            SAFE_BITWRITE(writer, cand->sequence[j], 8, file_to_write);
#ifdef DEBUG
            printf("[DEBUG] ➤ Written byte #%u: %02X\n", j, cand->sequence[j]);
            bitwriter_print_state(writer);
#endif
        }

        assigned[code_class]++;
    }

#ifdef DEBUG
    printf("[DEBUG] Final code map state:\n");
    print_code_map(&code_map);
    printf("[DEBUG] Header writing complete, flushing...\n");
#endif

    bitwriter_flush(writer);
    bitwriter_overwrite_at(writer, header_start_bit, candidate_count, 16);
    
    if (!bitwriter_write_to_file(writer, file_to_write)) {
        fprintf(stderr, "Failed to write header to file\n");
        exit(EXIT_FAILURE);
    }

#ifdef DEBUG
    printf("[DEBUG] Header written successfully (%d codes), BitWriter reset\n", candidate_count);
    bitwriter_print_state(writer);
#endif

    bitwriter_reset(writer);
    free_seq_freq_map(&seq_map);
    free(candidates);
}
// === FILE: /home/noman/takatuka/takatuka_compression/src/files/compression/compressed_header.h ===

#pragma once

#include "best_path_view.h"
#include "code_map.h"
#include "seq_freq_map.h"
#include <stdint.h>
#include <string.h>

extern CodeMap code_map;
// Function to populate header
void populate_header(BestPathView best_path, const uint8_t *block, FILE *file_to_write, BitWriter *writer);

// === FILE: /home/noman/takatuka/takatuka_compression/src/files/compression/compress.h ===

#pragma once

void write_compressed_output(const char* filename, const uint8_t* block);
                          
                          
