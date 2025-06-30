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
#include <stdio.h>
#include <stdbool.h>

#define MAX_LEVELS BLOCK_SIZE
#define MAX_WEIGHTS SEQ_LENGTH_LIMIT
#define INITIAL_GRAPH_NODES 500
#define GROWTH_FACTOR 2
#define MAX_SEQUENCES 64  // Adjust based on your needs (64 allows bitmask in uint64_t)
#define TOTAL_GRAPH_NODES(SEQ_LIMIT, LEVELS) \
    (1 + ((SEQ_LIMIT) * ((SEQ_LIMIT) + 1)) / 2 + ((LEVELS) - (SEQ_LIMIT)) * (SEQ_LIMIT))

#define MAX_GRAPH_NODES TOTAL_GRAPH_NODES(MAX_WEIGHTS, MAX_LEVELS)
typedef struct {
    uint32_t node_id;
    uint32_t offset;    
    uint16_t node_level;
    uint8_t sequence_length;
    uint8_t isUseless;
} GraphNode;

typedef struct {    
    uint32_t size;
    uint32_t first_node_of_level[MAX_LEVELS];
    GraphNode nodes[MAX_GRAPH_NODES];
    uint16_t total_levels;

} Graph;


void init_graph(void);
uint8_t get_parent_nodes_count(GraphNode* node);
uint32_t total_nodes_at_level(uint16_t level);
uint32_t get_level_start_id(uint16_t level);
uint32_t get_level_end_id(uint16_t level);
uint16_t get_last_level_index(void); 
GraphNode* get_graph_node(uint32_t node_id);
GraphNode* get_next_node(void);
void increment_graph_level(void);
uint32_t get_graph_node_count(void);
uint32_t get_graph_size(void);
GraphNode* get_parent_nodes(GraphNode* node);
void print_graph_node(GraphNode *node);
void print_node_sequence(GraphNode *node, const uint8_t* block);
#endif