// graph/graph.c

#include "graph.h"

// Static memory pool
static GraphNode nodes[GRAPH_MAX_NODES];
static bool initialized = false;
static uint32_t current_node_index = 0;


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

uint32_t get_current_graph_node_index() {
    return current_node_index;
}

bool graph_add_edge(uint32_t from, uint32_t to) {
    if (from >= GRAPH_MAX_NODES || to >= GRAPH_MAX_NODES) return false;
    
    GraphNode* src = &nodes[from];
    GraphNode* dst = &nodes[to];
    
    // Fast boundary checks
    if (src->child_count >= SEQ_LENGTH_LIMIT || 
        dst->parent_count >= SEQ_LENGTH_LIMIT) {
        return false;
    }
    
    // Single-pass insertion
    src->children[src->child_count++] = to;
    dst->parents[dst->parent_count++] = from;
    return true;
}


GraphNode* get_next_node() {
    if (current_node_index < GRAPH_MAX_NODES) {
        return &nodes[current_node_index++];
    } else {
        return NULL;
    }
}

uint32_t get_current_index() {

}

bool is_graph_full() {
    if (current_node_index >= GRAPH_MAX_NODES) {
        return true;
    } 
    return false;
}


void print_graph_node(const GraphNode *node, const uint8_t* block) {
    if (!node) {
        printf("NULL node\n");
        return;
    }
    printf("\n\nGraphNode @ %p:", (void*)node);
    printf("  id: %u", node->id);
    printf("  incoming_weight: %u", node->incoming_weight);
    printf(",  saving_so_far: %d", node->saving_so_far);
    for (int i=0; i < node->parent_count; i++) {
        printf("  parent_id: %u", node->parents[i]);
    }
    for (int i=0; i < node->child_count; i++) {
        printf("  parent_id: %u", node->children[i]);
    }
    
    int nodeCount = 0;
    for (int index=node->compress_start_index; index < node->compress_start_index+node->compress_sequence; index++) {
        printf("0x%x", block[index]);
    }
    /*todo    if (node->map) {
        binseq_map_print(node->map);
    } else {
        printf("\nMap is NULL\n");
    }*/
}
