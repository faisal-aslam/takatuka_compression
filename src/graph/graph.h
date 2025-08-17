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

extern Graph graph; //always use graph.c definiton.

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


