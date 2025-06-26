//graph.c

#include "graph.h"
#include <string.h>
#include <assert.h>
#include "graph_visualizer.h"

static Graph graph;

void init_graph(void) {
    memset(&graph, 0, sizeof(graph));
    graph.current_level = 0;
    
    // Initialize all nodes with sequential IDs
    for (uint32_t i = 0; i < TOTAL_NODES; i++) {
        graph.nodes[i].node_id = i;
    }
}

GraphNode* get_all_nodes_of_level(uint16_t level) {
    if (level >= MAX_LEVELS) return NULL;
    return &graph.nodes[level * MAX_WEIGHTS]; // Pointer to first node in level
}

uint16_t get_current_level(void) {
    return graph.current_level;
}

void add_link_to_parent(GraphNode* child_node, GraphNode* parent_node, uint8_t cost) {
    if (!child_node || !parent_node || child_node->parent_count >= MAX_WEIGHTS) 
        return;
    
    child_node->parent_link[child_node->parent_count++] = (ParentLink){
        .parent_id = parent_node->node_id,
        .cost = cost
    };
}

GraphNode* get_graph_node(uint32_t node_id) {
    return (node_id < TOTAL_NODES) ? &graph.nodes[node_id] : NULL;
}

void increment_graph_level() {
    graph.current_level++;
}

const Graph* get_graph(void) {
    return &graph; // Return const pointer to prevent modification
}

uint32_t get_graph_node_count(void) {
    return graph.current_level * MAX_WEIGHTS;
}
static inline void print_node_link(GraphNode* node, ParentLink* link) {
    printf("\t %u-->%u", node->node_id, link->parent_id);
}
void print_graph_node(GraphNode *node) {
    if (!node) return; //nothing to print.
    if (node->node_id == 0) {
        printf("\n ROOT ");
    } else {
        printf("\n ");
    }
    printf(" node_id=%u, start_of_sequence=%u, sequence_length=%u", node->node_id, node->start_of_sequence, node->sequence_length);
    printf(", parent_count= %u\n", node->parent_count);
    for (int loop = 0; loop < node->parent_count; loop ++) {
        print_node_link(node, &node->parent_link[loop]);
    }
}