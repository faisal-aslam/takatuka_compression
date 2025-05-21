// graph.c
#include "graph.h"

// Static memory pool
static GraphNode nodes[GRAPH_MAX_NODES];
static bool initialized = false;

void graph_init(void) {
    if (initialized) return;
    initialized = true;
    memset(nodes, 0, sizeof(nodes)); // Blazing fast zeroing
    
    // Compiler will optimize this loop
    for (uint32_t i = 0; i < GRAPH_MAX_NODES; i++) {
        nodes[i].id = i;
    }
}


GraphNode* graph_get_node(uint32_t index) {
    return (index < GRAPH_MAX_NODES) ? &nodes[index] : NULL;
}

bool graph_add_edge(uint32_t from, uint32_t to) {
    if (from >= GRAPH_MAX_NODES || to >= GRAPH_MAX_NODES) return false;
    
    GraphNode* src = &nodes[from];
    GraphNode* dst = &nodes[to];
    
    // Fast boundary checks
    if (src->child_count >= GRAPH_MAX_CONNECTIONS || 
        dst->parent_count >= GRAPH_MAX_CONNECTIONS) {
        return false;
    }
    
    // Single-pass insertion
    src->children[src->child_count++] = to;
    dst->parents[dst->parent_count++] = from;
    return true;
}