#include "graph.h"
#include <string.h>
#include <assert.h>

// Fast index calculation macro
#define NODE_INDEX(level, weight) ((level) * MAX_WEIGHTS + (weight))

// Static graph object - only accessible within this file
static Graph graph;

void init_graph(void) {
    memset(&graph, 0, sizeof(graph));
    graph.current_level = 0;
    
    // Initialize node IDs
    for (uint16_t level = 0; level < MAX_LEVELS; level++) {
        for (uint16_t weight = 0; weight < MAX_WEIGHTS; weight++) {
            graph.nodes[NODE_INDEX(level, weight)].node_id = NODE_INDEX(level, weight);
        }
    }
}

GraphNode* get_all_nodes_of_level(uint16_t level) {
    if (level >= MAX_LEVELS) return NULL;
    return &graph.nodes[NODE_INDEX(level, 0)]; // Returns pointer to first node in level
}

uint16_t get_current_level(void) {
    return graph.current_level;
}

void add_link_to_parent(GraphNode* child_node, GraphNode* parent_node, uint8_t weight, uint8_t cost) {
    if (child_node == NULL || parent_node == NULL) return;
    if (child_node->parent_count >= MAX_WEIGHTS) return;
    
    ParentLink new_link = {
        .parent_id = parent_node->node_id,
        .weight = weight,
        .cost = cost
    };
    
    child_node->parent_link[child_node->parent_count++] = new_link;
}

GraphNode* get_graph_node(uint32_t node_id) { 
    if (node_id >= MAX_LEVELS * MAX_WEIGHTS) return NULL;
    return &graph.nodes[node_id];
}