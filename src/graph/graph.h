#ifndef NEURALNET_GRAPH_H
#define NEURALNET_GRAPH_H
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
#include "../map/sequence_repository_freq.h"

#define MAX_LEVELS BLOCK_SIZE
#define MAX_WEIGHTS SEQ_LENGTH_LIMIT

extern SequenceRepository exist_repo[MAX_LEVELS];

#define TOTAL_GRAPH_NODES(SEQ_LIMIT, LEVELS) \
    (1 + ((SEQ_LIMIT) * ((SEQ_LIMIT) + 1)) / 2 + ((LEVELS) - (SEQ_LIMIT)) * (SEQ_LIMIT))

#define MAX_GRAPH_NODES TOTAL_GRAPH_NODES(MAX_WEIGHTS, MAX_LEVELS)
typedef struct {
    uint32_t node_id;
    uint32_t offset;    
    uint32_t hash_index_cache; //for frequency hash (not for binary hash)
    uint16_t node_level;
    uint16_t min_depth;
    uint8_t sequence_length;
    uint8_t isUseless;
} GraphNode;

typedef struct {    
    uint32_t size;
    uint32_t first_node_of_level[MAX_LEVELS];    
    GraphNode nodes[MAX_GRAPH_NODES];
    uint16_t total_levels;
    uint16_t level_min_depth[MAX_LEVELS]; // computed during compaction
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
static inline void increment_graph_level(void);
static inline uint32_t get_graph_size(void);
static inline GraphNode* get_parent_nodes(GraphNode* node);
static inline uint16_t get_parent_level(GraphNode* node);
static inline void reset_graph(void);
void print_graph_node(GraphNode *node);
void print_node_sequence(GraphNode *node, const uint8_t* block);
void compact_graph(const uint8_t* block);

static inline void reset_graph(void) {
    graph.size = 0;
    graph.total_levels = 0;
}

static inline GraphNode* get_next_node(void) {
    GraphNode* g_node = &graph.nodes[graph.size++];
    g_node->node_id = graph.size-1; //please never change node's id ever.
    g_node->node_level = graph.total_levels-1; //please do not change this ever too.  
    g_node->hash_index_cache = UINT32_MAX;  
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
    return graph.first_node_of_level[level + 1];
}

static inline uint16_t get_last_level_index(void)  {
    return (graph.total_levels > 0) ? graph.total_levels - 1 : 0;
}

static inline uint16_t get_parent_level(GraphNode* node) {
    uint16_t parent_level =  node->node_level-node->sequence_length; 
    if (parent_level == UINT16_MAX ||  parent_level > MAX_LEVELS) {
        fprintf(stderr, "illegal parent level");
        exit(1);
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


static inline void increment_graph_level(void) {
    if (graph.total_levels < MAX_LEVELS) {
        graph.first_node_of_level[graph.total_levels] = graph.size;
        graph.total_levels++;
    }
}


static inline uint32_t total_nodes_at_level(uint16_t level) {
    return (get_level_end_id(level) - get_level_start_id(level));
}

uint8_t get_parent_nodes_count(GraphNode* node) {
    if (!node || node->node_id == 0) {
        return 0;
    }
    uint8_t parents_count = total_nodes_at_level(get_parent_level(node));
    if (parents_count > SEQ_LENGTH_LIMIT) {
        fprintf(stderr, "Illegal number of parent nodes");
        exit(1);
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


#endif