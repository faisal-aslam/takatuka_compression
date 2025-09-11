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
    uint32_t best_savings;
    uint16_t node_level;
    uint8_t useless;
    uint8_t sequence_length;
    uint8_t is_RLE;
    //uint8_t repeat_seq_length;
    uint8_t length_of_RLE;
} GraphNode;

typedef struct {    
    uint32_t size;
    uint32_t first_node_of_level[MAX_LEVELS];
    GraphNode nodes[MAX_GRAPH_NODES];
    uint16_t total_levels;
} Graph;

typedef enum {
    LEVEL_ACTIVE = 0,        // normal, nothing special
    LEVEL_DELETED,         // level is deleted
    LEVEL_DONE,           // processed/finished
} LevelStatus;

extern Graph graph; //always use graph.c definiton.

extern LevelStatus level_status[MAX_LEVELS]; 

void init_graph(void);
static inline uint8_t get_parent_nodes_count(GraphNode* node);
static inline uint32_t total_nodes_at_level(uint16_t level);
static inline uint32_t get_level_start_id(uint16_t level);
static inline uint32_t get_level_end_id(uint16_t level);
static inline uint16_t get_last_level_index(void); 
static inline GraphNode* get_graph_node(uint32_t node_id);
static inline GraphNode* get_next_node(void);
static inline uint16_t create_graph_level(void);
static inline uint32_t get_graph_size(void);
static inline GraphNode* get_parent_nodes(GraphNode* node);
static inline uint16_t get_parent_level(GraphNode* node);
static inline void reset_graph(void);
static inline uint8_t get_parent_nodes_count_by_level_and_length(uint16_t level, uint8_t seq_length);
void print_graph_node(GraphNode *node);
void print_node_sequence(GraphNode *node, const uint8_t* block);
void print_all_nodes(const uint8_t* block);
void mass_increment_levels(int add_levels);
void compact_graph(const uint8_t* block);
void rebuild_seq_freq_map(const uint8_t *block, uint8_t avoid_done_levels);
void mark_single_freq_nodes_useless(const uint8_t* block);

/**
 * @brief Detects simple Run-Length Encodable (RLE) sequences in a data block.
 *
 * The function checks for two types of RLE sequences starting at the given offset:
 *   1. Uniform sequences (e.g., "AAAAAA").
 *   2. Arithmetic +1 sequences (e.g., "ABCDE", "56789").
 *
 * Only prefixes of length >= MIN_RLE_SEQ_LENGTH are considered valid.
 *
 * Output Parameters:
 * - repeat_seq_length: For both uniform and arithmetic sequences = 1.
 * - length_of_RLE: Length of the detected sequence, or 0 if none found.
 * - rle_type: 0 = not RLE, 1 = uniform RLE, 2 = arithmetic +1 RLE.
 *
 * @param[out] repeat_seq_length Unit length of repeating sequence (always 1 here).
 * @param[out] length_of_RLE Number of bytes encodable with RLE (0 if not found).
 * @param[in] block_size Size of the block region to analyze.
 * @param[in] offset Start offset within the block.
 * @param[in] block Pointer to the data block.
 * @param[out] rle_type Type of RLE found (0=none, 1=uniform, 2=arithmetic+1).
 *
 * @note Stops at the first valid match (uniform checked before arithmetic).
 *
 * @example "AAAAAB" → rle_type=1, repeat_seq_length=1, length_of_RLE=5
 * @example "ABCDEZ" → rle_type=2, repeat_seq_length=1, length_of_RLE=5
 * @example "AXYZ"   → rle_type=0, length_of_RLE=0
 */
void is_RLE_sequence(uint8_t* repeat_seq_length, uint8_t* length_of_RLE,
                     uint8_t block_size, uint32_t offset,
                     const uint8_t *block, uint8_t* rle_type);

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


static inline uint16_t create_graph_level(void) {
    if (graph.total_levels < MAX_LEVELS) {
        graph.first_node_of_level[graph.total_levels] = graph.size;
        graph.total_levels++;
        return graph.total_levels-1;
    }
    fprintf(stderr, "Illegal level created \n");
    abort();
    return 0;
}

static inline uint8_t get_parent_nodes_count_by_level_and_length(uint16_t level, uint8_t seq_length) {
    // Ensure the level is valid and large enough for a sequence of length `seq_length`
    if (level == 0 || seq_length == 0 || seq_length > level) {
        return 0;
    }

    uint16_t parent_level = level - seq_length;

    if (parent_level >= graph.total_levels) {
        return 0;
    }

    uint32_t count = total_nodes_at_level(parent_level);

    if (count > SEQ_LENGTH_LIMIT + 1) {
        fprintf(stderr, "Illegal number of parent nodes at hypothetical level=%u (seq_length=%u)\n",
                level, seq_length);
        abort();
    }

    return (uint8_t)count;
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


