//graph.h

#ifndef NEURALNET_GRAPH_H
#define NEURALNET_GRAPH_H

#include <stdint.h>
#include "../constants.h"
#include <stdio.h>

#define MAX_LEVELS BLOCK_SIZE
#define MAX_WEIGHTS SEQ_LENGTH_LIMIT
#define TOTAL_NODES (MAX_LEVELS * MAX_WEIGHTS)

typedef struct {
    uint32_t parent_id;
    uint8_t cost;
} ParentLink;

typedef struct {
    uint32_t node_id;
    uint32_t start_of_sequence;
    ParentLink parent_link[MAX_WEIGHTS];
    uint8_t parent_count;
    uint8_t sequence_length;    
} GraphNode;

typedef struct {
    GraphNode nodes[TOTAL_NODES];
    uint16_t current_level;
} Graph;

void init_graph(void);
GraphNode* get_all_nodes_of_level(uint16_t level);
uint16_t get_current_level(void);
void add_link_to_parent(GraphNode* child_node, GraphNode* parent_node, uint8_t cost);
GraphNode* get_graph_node(uint32_t node_id);
void increment_graph_level();
const Graph* get_graph(void);
uint32_t get_graph_node_count(void);
void print_graph_node(GraphNode *node);

#endif