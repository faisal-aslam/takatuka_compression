// Combined C and H Files

// === FILE: /home/noman/github/takatuka_compression/src/logic.c ===

#include "logic.h"
#include "compress.h"
#include "decompress.h"
#include "graph.h"
#include "graph_visualizer.h"
#include "seq_freq_map.h"
#include "shortest_path.h"
#include "timer.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static GraphNode *root_node = NULL;

static inline void create_root() {
    create_graph_level();
    root_node = get_next_node();
    assert(root_node != NULL);
}
// use to store
typedef struct {
    uint32_t RLE_offset;       // The starting point of block of RLE
    uint16_t next_RLE_level;   // we must wait till the next RLE level to create the node.
    uint8_t repeat_seq_length; // number of bytes repeated. Like for ABCABCABCABC repeat_seq_length=3
    uint8_t length_of_RLE;     // length of RLE sequence. Like for ABCABCABCABC length_of_RLE=4
} RLE_info;

RLE_info rle_info;

static inline GraphNode *create_node(uint32_t start, uint8_t length) {
    GraphNode *node = get_next_node();
    if (!node) return NULL;

    node->offset = start;
    node->sequence_length = length;
    return node;
}

static inline uint8_t RLE_logic(const uint8_t *block, uint32_t block_index, uint32_t block_size) {
    // Create RLE node, if any. There could be at most one RLE node per level.
    uint16_t current_level = get_last_level_index();
    if (rle_info.next_RLE_level < current_level && is_RLE_sequence(&rle_info.repeat_seq_length, &rle_info.length_of_RLE,
                                                                   MIN(block_size, 255), block_index, block)) {
        // wait for the right level to create node.
        // do not create any RLE nodes before reaching that level.
        // remember data of RLE node to be created later on, at the appropriate level.
        rle_info.next_RLE_level = current_level + rle_info.length_of_RLE - 1;
        rle_info.RLE_offset = block_index;
        return;
    }
    GraphNode *current_node;
    if (current_level == rle_info.next_RLE_level) {
        current_node = create_node(rle_info.RLE_offset, rle_info.length_of_RLE);
        current_node->is_RLE = 1;
        current_node->repeat_seq_length = rle_info.repeat_seq_length;
        current_node->length_of_RLE = rle_info.length_of_RLE;
#ifdef DEBUG
        print_graph_node(current_node); // print the RLE node.
#endif
        return 1;
    }
    return 0;
}

void process_block(const uint8_t *block, uint32_t block_size) {

    init_graph();
    init_seq_freq_map();
    create_root();

#ifdef DEBUG
    print_graph_node(get_graph_node(0));
#endif
    for (uint32_t block_index = 0; block_index < block_size; block_index++) {
        GraphNode *current_node = NULL;
        create_graph_level(); // create new level of the graph
        uint16_t current_level = get_last_level_index();

        if (current_level >= MAX_LEVELS) {
            fprintf(stderr, "Number of levels are more than allowed\n");
            abort();
        }
        uint8_t max_sequence = MIN(current_level, MAX_WEIGHTS);
        uint8_t start;
        uint8_t created_rle_node = RLE_logic(block, block_index, block_size);
        if (created_rle_node){
            max_sequence = 1;
        }
        // Make sequences of specific sizes.
        for (uint8_t seq_len = 1; seq_len <= max_sequence; seq_len++) {
            start = block_index - seq_len + 1;
            current_node = create_node(start, seq_len);
            if (seq_len > 1) {
                seq_freq_increment(&block[current_node->offset], seq_len);
            }
#ifdef DEBUG
            print_graph_node(current_node); // print the newly create node.
#endif
        }
        
    }
    
#ifdef DEBUG
    visualize_graph(block); // create graph in DOT for visualization.
#endif
    find_shortest_path_to_sink(block);
}

// === FILE: /home/noman/github/takatuka_compression/src/graph/graph.h ===

#pragma once
/*
 * Graph structure with virtual parent links:
 * - Nodes are organized in levels.
 * - Each node’s parents are all nodes in the previous level whose sequences may match.
 * - Parent links are inferred on-the-fly based on level and sequence_length.
 * - This saves memory and allows fast traversal by computing parent ranges.
 */

#include <stdint.h>
#include "../constants.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "graph_visualizer.h"
#include <stdio.h>
#include <stdbool.h>

#define MAX_LEVELS (BLOCK_SIZE+1) //one extra for the root level.
#define MAX_WEIGHTS SEQ_LENGTH_LIMIT


typedef struct {
    uint32_t node_id;
    uint32_t offset;    
    uint16_t node_level;
    uint8_t useless;
    uint8_t sequence_length;
    uint8_t is_RLE;
    uint8_t repeat_seq_length;
    uint8_t length_of_RLE;
} GraphNode;

typedef struct {    
    uint32_t size;
    uint32_t first_node_of_level[MAX_LEVELS];    
    GraphNode nodes[MAX_GRAPH_NODES];
    uint16_t total_levels;
} Graph;

extern Graph graph; //always use graph.c definiton.

void init_graph(void);
static inline uint8_t get_parent_nodes_count(GraphNode* node);
static inline uint32_t total_nodes_at_level(uint16_t level);
static inline uint32_t get_level_start_id(uint16_t level);
static inline uint32_t get_level_end_id(uint16_t level);
static inline uint16_t get_last_level_index(void); 
static inline GraphNode* get_graph_node(uint32_t node_id);
static inline GraphNode* get_next_node(void);
static inline uint8_t create_graph_level(void);
static inline uint32_t get_graph_size(void);
static inline GraphNode* get_parent_nodes(GraphNode* node);
static inline uint16_t get_parent_level(GraphNode* node);
static inline void reset_graph(void);
void print_graph_node(GraphNode *node);
void print_node_sequence(GraphNode *node, const uint8_t* block);
void print_all_nodes(const uint8_t* block);
void mass_increment_levels(int add_levels);
void compact_graph(const uint8_t* block, uint8_t* levels_to_keep);

/**
 * @brief Detects Run-Length Encodable (RLE) sequences within a data block
 * 
 * This function analyzes a block of data to identify the longest prefix suitable for RLE compression,
 * either as a uniform byte sequence or a repeating pattern. The function is optimized for performance
 * when processing entire blocks at once.
 * 
 * Key Features:
 * - Detects both uniform sequences (e.g., "AAAAA") and patterned sequences (e.g., "ABABAB")
 * - Returns the longest valid RLE prefix meeting minimum length requirements
 * - Processes data in-place without memory allocation
 * - Uses optimized checks for early rejection of non-RLE candidates
 * 
 * Output Parameters:
 * - repeat_seq_length: For uniform sequences = 1, for patterns = pattern length
 * - length_of_RLE: Number of bytes that can be RLE encoded (may be less than block_size)
 * 
 * @param[out] repeat_seq_length Length of repeating pattern (1 for uniform sequences)
 * @param[out] length_of_RLE Length of encodable sequence (0 if no RLE found)
 * @param[in] block_size Total size of the block to analyze
 * @param[in] offset Byte offset within the block to start analysis
 * @param[in] block Pointer to the data block
 * 
 * @return uint8_t Returns 1 if RLE sequence found, 0 otherwise
 * 
 * @note Performance Considerations:
 *       - Processes data in a single pass when possible
 *       - Uses memcmp for efficient pattern comparison
 *       - Early termination on non-RLE sequences
 * 
 * @example "AAAAAAABCD" → returns 1, repeat_seq_length=1, length_of_RLE=7
 * @example "ABABABXXXX" → returns 1, repeat_seq_length=2, length_of_RLE=6
 * @example "ABCDEFGHIJ" → returns 0
 * 
 * @see MIN_RLE_SEQ_LENGTH Minimum sequence length to consider for RLE
 * @see RLE_MAX_PATTERN_LENGTH Maximum pattern length to check
 */
uint8_t is_RLE_sequence(uint8_t* repeat_seq_length, uint8_t* length_of_RLE, uint8_t block_size, uint32_t offset, const uint8_t *block);

static inline void reset_graph(void) {
    graph.size = 0;
    graph.total_levels = 0;
}

static inline GraphNode* get_next_node(void) {
    GraphNode* g_node = &graph.nodes[graph.size++];
    g_node->node_id = graph.size-1; //please never change node's id ever.
    g_node->node_level = graph.total_levels-1; //please do not change this ever too.  
    return g_node;
}

static inline uint32_t get_level_start_id(uint16_t level) {
    if (level >= graph.total_levels) {
        fprintf(stderr, "Illegal level: %u (total_levels=%u)\n", level, graph.total_levels);
        exit(1);
    }
    return graph.first_node_of_level[level];
}

static inline uint32_t get_level_end_id(uint16_t level) {
    if (level >= graph.total_levels) {
        fprintf(stderr, "Illegal level: %u (total_levels=%u)\n", level, graph.total_levels);
        exit(1);
    }
    if (level == graph.total_levels - 1) {
        return graph.size;
    }
    return graph.first_node_of_level[level+1];
}

static inline uint16_t get_last_level_index(void)  {
    return (graph.total_levels > 0) ? graph.total_levels - 1 : 0;
}

static inline uint16_t get_parent_level(GraphNode* node) {    
    uint16_t parent_level =  node->node_level-node->sequence_length; 
    if (!node || parent_level == UINT16_MAX ||  parent_level > MAX_LEVELS) {
        fprintf(stderr, "illegal parent level");
        abort();
    }
    return parent_level;
}

static inline uint32_t get_graph_size(void) {
    return graph.size;
}

static inline GraphNode* get_graph_node(uint32_t node_id) {
    assert(node_id < graph.size);
    return &graph.nodes[node_id];
}


static inline uint8_t create_graph_level(void) {
    if (graph.total_levels < MAX_LEVELS) {
        graph.first_node_of_level[graph.total_levels] = graph.size;
        graph.total_levels++;
        return 1;
    }
    return 0;
}


static inline uint32_t total_nodes_at_level(uint16_t level) {
    return (get_level_end_id(level) - get_level_start_id(level));
}

uint8_t get_parent_nodes_count(GraphNode* node) {
    if (!node || node->node_id == 0) {
        return 0;
    }
    uint8_t parents_count = total_nodes_at_level(get_parent_level(node));
    if (parents_count > SEQ_LENGTH_LIMIT+1) {
        fprintf(stderr, "Illegal number of parent nodes, at node=%d, node_level=%d\n", node->node_id, node->node_level);        
        abort();
    }
    return parents_count;
}

static inline GraphNode* get_parent_nodes(GraphNode* node) {
    if (node->node_id == 0) return NULL;
    //Step 1: Get parent level.
    uint16_t parent_level = get_parent_level(node);
    // Step 2: Get the index of the first node of the parent level
    uint32_t start_index =get_level_start_id (parent_level);
    return &graph.nodes[start_index];
}



// === FILE: /home/noman/github/takatuka_compression/src/graph/graph.c ===

#include "graph.h"
#include "seq_freq_map.h"
#include "timer.h"



#define PER_LEVEL_GRAPH_NODES(SEQ_LIMIT, LEVEL)                                                                        \
    ((LEVEL) == 0               ? 1                                                                                    \
     : ((LEVEL) <= (SEQ_LIMIT)) ? ((LEVEL) * ((LEVEL) + 1)) / 2                                                        \
                                : ((SEQ_LIMIT) * ((SEQ_LIMIT) + 1)) / 2 + ((LEVEL) - (SEQ_LIMIT)) * (SEQ_LIMIT))

#define MIN_RLE_SEQ_LENGTH 10
#define RLE_MAX_PATTERN_LENGTH 3 // such as abcabcabc...

Graph graph; // Actual single definition

void init_graph(void) {
    graph.size = 0;
    graph.total_levels = 0;
    memset(graph.first_node_of_level, 0, sizeof(graph.first_node_of_level));
}

void mass_increment_levels(int add_levels) {
    if (graph.total_levels + add_levels < MAX_LEVELS) {
        for (int i = 0; i < add_levels; i++) {
            graph.first_node_of_level[graph.total_levels + i] = UINT32_MAX; // no node at this level.
        }
        graph.total_levels += add_levels;
    }
}

uint8_t is_RLE_sequence(uint8_t *repeat_seq_length, uint8_t *length_of_RLE, uint8_t block_size, uint32_t offset,
                        const uint8_t *block) {
    *repeat_seq_length = 0;
    *length_of_RLE = 0;

    if (block_size < MIN_RLE_SEQ_LENGTH) {
        return 0;
    }

    const uint8_t *sequence = block + offset;
    const uint8_t first_byte = sequence[0];

    // ===== Stage 1: Uniform Sequence Check (for whole sequence or prefix) =====
    uint8_t uniform_length = block_size;

    // Find the first position where the byte differs
    for (uint8_t i = 1; i < block_size; i++) {
        if (sequence[i] != first_byte) {
            uniform_length = i;
            break;
        }
    }

    if (uniform_length >= MIN_RLE_SEQ_LENGTH) {
        *repeat_seq_length = 1;
        *length_of_RLE = uniform_length;

#ifdef DEBUG
        printf("[RLE] Uniform sequence found at offset %u: repeat_len = %u, RLE_len = %u\n",
               offset, *repeat_seq_length, *length_of_RLE);
#endif

        return 1;
    }

    // ===== Stage 2: Pattern-Based RLE Check (for whole sequence or prefix) =====
    if (block_size < 16) {
        return 0;
    }

    // Quick entropy filter: check uniqueness among first 4 bytes
    bool is_unique = true;
    for (int i = 0; i < 4 && is_unique; i++) {
        for (int j = i + 1; j < 4; j++) {
            if (sequence[i] == sequence[j]) {
                is_unique = false;
                break;
            }
        }
    }
    if (is_unique) {
        return 0;
    }

    int max_pattern = MIN(block_size / 2, RLE_MAX_PATTERN_LENGTH);

    for (int pattern_len = max_pattern; pattern_len >= 2; pattern_len--) {
        int max_valid_repeats = 1; // start with 1 pattern already seen
        bool valid = true;

        while (valid) {
            int base = (max_valid_repeats - 1) * pattern_len;
            int next = base + pattern_len;

            if (next + pattern_len > block_size - offset) {
                break;
            }

            for (int i = 0; i < pattern_len; i++) {
                if (sequence[base + i] != sequence[next + i]) {
                    valid = false;
                    break;
                }
            }

            if (valid) {
                max_valid_repeats++;
            }
        }

        int total_length = max_valid_repeats * pattern_len;

        if (max_valid_repeats >= 2 && total_length >= MIN_RLE_SEQ_LENGTH) {
            *repeat_seq_length = pattern_len;
            *length_of_RLE = total_length;

#ifdef DEBUG
            printf("[RLE] Pattern sequence found at offset %u: repeat_len = %u, RLE_len = %u, repeats = %d\n",
                   offset, *repeat_seq_length, *length_of_RLE, max_valid_repeats);
#endif

            return 1;
        }
    }

    return 0;
}

static inline void print_node_link(GraphNode *node, GraphNode *parent) {
    printf("\t %u --> %u\n", node->node_id, parent->node_id);
}

void print_node_sequence(GraphNode *node, const uint8_t *block) {
    for (int i = 0; i < node->sequence_length; i++) {
        printf("%c", block[node->offset + i]);
        if (i + 1 < node->sequence_length) {
            printf(",");
        }
    }
    printf("\n");
}

void print_graph_node(GraphNode *node) {
    if (!node) return;

    if (node->node_id == 0) {
        printf("\nROOT NODE ");
    } else {
        printf("\n");
    }
    uint16_t parent_nodes_count = get_parent_nodes_count(node);
    printf("node_id = %u, start_of_sequence = %u, sequence_length = %u, level=%u", node->node_id, node->offset,
           node->sequence_length, node->node_level);
    if (!node->is_RLE) {
        printf(", parent_count = %u\n", parent_nodes_count);
    } else {
        printf(", RLE=YES, parent_count = %u\n", parent_nodes_count);
    }
    GraphNode *parent_nodes = get_parent_nodes(node);
    if (!parent_nodes) return;
    for (int i = 0; i < parent_nodes_count; i++) {
        print_node_link(node, &parent_nodes[i]);
        break; // just print one link per node as other belings to the same level.
    }
}

void print_all_nodes(const uint8_t *block) {
    for (uint32_t i = 0; i < graph.size; i++) {
        GraphNode *node = &graph.nodes[i];
        print_graph_node(node);
        print_node_sequence(node, block);
    }
}
// === FILE: /home/noman/github/takatuka_compression/src/map/seq_freq_map.c ===

//seq_freq_map.c

#include "seq_freq_map.h"
#include "xxhash.h"
#include "general_map.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define META_ENCODE(freq, len)  (((uint32_t)(len) << 24) | ((freq) & 0xFFFFFF))
#define META_GET_FREQ(meta)     ((meta) & 0xFFFFFF)
#define META_GET_LEN(meta)      ((uint8_t)((meta) >> 24))

static uint32_t freq_one_count = 0;

typedef struct {
    const uint8_t *sequence;  // 8 bytes: external pointer
    uint32_t meta;            // 4 bytes: upper 8 bits = length, lower 24 bits = frequency
} SeqFreqEntry;

typedef struct SeqFreqMap {
    SeqFreqEntry entries[SEQ_MAP_CAPACITY];
} SeqFreqMap;

SeqFreqMap seqMap;

void init_seq_freq_map(void) {
    memset(&seqMap, 0, sizeof(SeqFreqMap));
    freq_one_count = 0;
}

static inline uint32_t find_slot(const uint8_t *seq, uint8_t len, uint64_t hash, int *found) {
    uint32_t idx = hash % SEQ_MAP_CAPACITY;
    uint32_t start_idx = idx;

    do {
        const SeqFreqEntry *entry = &seqMap.entries[idx];

        if (entry->sequence == NULL) {
            *found = 0;
            return idx;
        }

        if (META_GET_LEN(entry->meta) == len &&
            sequences_equal(entry->sequence, seq, len)) {
            *found = 1;
            return idx;
        }

        idx = (idx + 1) % SEQ_MAP_CAPACITY;
    } while (idx != start_idx);

    fprintf(stderr, "No free slot in SeqFreqMap\n");
    abort();
}

uint32_t seq_freq_increment(const uint8_t *seq, uint8_t len) {
    uint64_t hash = XXH3_64bits_withSeed(seq, len, 0);
    int found;
    uint32_t idx = find_slot(seq, len, hash, &found);

    SeqFreqEntry *entry = &seqMap.entries[idx];

    if (found) {
        uint32_t old_freq = META_GET_FREQ(entry->meta);
        uint32_t new_freq = old_freq + 1;
        entry->meta = META_ENCODE(new_freq, len);

        if (old_freq == 1) freq_one_count--;  // went from 1 to >1
        return new_freq;
    } else {
        entry->sequence = seq;
        entry->meta = META_ENCODE(1, len);
        freq_one_count++;
        return 1;
    }
}

uint32_t seq_freq_set(const uint8_t *seq, uint8_t len, uint32_t freq) {
    if (freq > 0xFFFFFF) {
        fprintf(stderr, "Frequency exceeds 24-bit limit\n");
        abort();
    }

    uint64_t hash = XXH3_64bits_withSeed(seq, len, 0);
    int found;
    uint32_t idx = find_slot(seq, len, hash, &found);

    SeqFreqEntry *entry = &seqMap.entries[idx];
    uint32_t old_freq = found ? META_GET_FREQ(entry->meta) : 0;

    entry->sequence = seq;
    entry->meta = META_ENCODE(freq, len);

    if (old_freq == 1 && freq != 1) freq_one_count--;
    else if (old_freq != 1 && freq == 1) freq_one_count++;
    else if (!found && freq == 1) freq_one_count++;

    return freq;
}


uint32_t seq_freq_decrement(const uint8_t *seq, uint8_t len) {
    uint64_t hash = XXH3_64bits_withSeed(seq, len, 0);
    int found;
    uint32_t idx = find_slot(seq, len, hash, &found);

    if (!found) {
        fprintf(stderr, "Sequence not found for decrement\n");
        abort();
    }

    SeqFreqEntry *entry = &seqMap.entries[idx];
    uint32_t old_freq = META_GET_FREQ(entry->meta);

    if (old_freq == 0) {
        fprintf(stderr, "Invalid decrement — frequency already 0\n");
        abort();
    }

    uint32_t new_freq = old_freq - 1;

    if (old_freq == 1) {
        freq_one_count--;  // from 1 to 0
        entry->sequence = NULL;
        entry->meta = 0;
        return 0;
    } else {
        if (new_freq == 1) freq_one_count++;  // from >1 to 1
        entry->meta = META_ENCODE(new_freq, len);
        return new_freq;
    }
}

uint32_t seq_freq_one_count(void) {
    return freq_one_count;
}


uint32_t seq_freq_get(const uint8_t *seq, uint8_t len) {
    uint64_t hash = XXH3_64bits_withSeed(seq, len, 0);
    int found;
    uint32_t idx = find_slot(seq, len, hash, &found);

    return found ? META_GET_FREQ(seqMap.entries[idx].meta) : 0;
}

// === FILE: /home/noman/github/takatuka_compression/src/map/seq_freq_map.h ===

//seq_freq_map.h

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "constants.h"

#define SEQ_MAP_CAPACITY ((TOTAL_GRAPH_NODES(SEQ_LENGTH_LIMIT, BLOCK_SIZE) * 3) / 2)

void init_seq_freq_map(void);

uint32_t seq_freq_increment(const uint8_t *seq, uint8_t len);
uint32_t seq_freq_decrement(const uint8_t *seq, uint8_t len);
uint32_t seq_freq_get(const uint8_t *seq, uint8_t len);
uint32_t seq_freq_set(const uint8_t *seq, uint8_t len, uint32_t freq);
uint32_t seq_freq_one_count(void);
