#ifndef NEURALNET_GRAPH_H
#define NEURALNET_GRAPH_H

#include <stdint.h>
#include "../constants.h"

#define MAX_LEVELS BLOCK_SIZE
#define MAX_WEIGHTS SEQ_LENGTH_LIMIT
#define TOTAL_NODES (MAX_LEVELS * MAX_WEIGHTS)

typedef struct {
    uint32_t parent_id;
    uint8_t weight;
    uint8_t cost;
} ParentLink;

typedef struct {
    uint32_t node_id;
    ParentLink parent_link[MAX_WEIGHTS];
    uint8_t parent_count;
} GraphNode;

typedef struct {
    GraphNode nodes[TOTAL_NODES];  // 1D array
    uint16_t current_level;
} Graph;

void init_graph(void);
GraphNode* get_all_nodes_of_level(uint16_t level);
uint16_t get_current_level(void);
void add_link_to_parent(GraphNode* child_node, GraphNode* parent_node, uint8_t weight, uint8_t cost);
GraphNode* get_graph_node(uint32_t node_id);

#endif