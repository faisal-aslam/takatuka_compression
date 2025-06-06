// graph/graph.c
#include "graph.h"
#include <stdio.h>
#include <string.h>

Graph graph = {0};

/**
 * Initializes the graph structure with default values.
 * 
 * This function:
 * - Sets all graph fields to zero initially
 * - Assigns permanent, immutable IDs to each node (0 to GRAPH_MAX_NODES-1)
 * - Initializes the weight-level index structure
 * - Marks the graph as initialized to prevent reinitialization
 * 
 * The node IDs are designed to be immutable after initialization to maintain
 * consistency throughout the graph's lifetime.
 */
void graph_init(void) {
    if (graph.initialized) return;
    
    // Clear entire graph structure
    memset(&graph, 0, sizeof(graph));
    
    // Assign permanent IDs to nodes
    for (uint32_t i = 0; i < GRAPH_MAX_NODES; i++) {
        graph.nodes[i].id = i;
    }
    
    // Initialize weight-level index structure
    for (int w = 0; w < MAX_WEIGHT; w++) {
        for (int l = 0; l < MAX_LEVELS; l++) {
            graph.index.slots[w][l].indices = NULL;
            graph.index.slots[w][l].count = 0;
            graph.index.slots[w][l].capacity = 0;
        }
    }
    graph.index.max_level = 0;
    
    // Mark graph as initialized (must be last operation)
    graph.initialized = true;
}

GraphNode* graph_get_node(uint32_t index) {
    return (index < GRAPH_MAX_NODES) ? &graph.nodes[index] : NULL;
}

bool graph_add_edge(uint32_t from, uint32_t to) {
    if (from >= GRAPH_MAX_NODES || to >= GRAPH_MAX_NODES) return false;
    
    GraphNode* src = &graph.nodes[from];
    GraphNode* dst = &graph.nodes[to];
    
    if (src->child_count >= SEQ_LENGTH_LIMIT || 
        dst->parent_count >= SEQ_LENGTH_LIMIT) {
        return false;
    }
    
    src->children[src->child_count++] = to;
    dst->parents[dst->parent_count++] = from;
    return true;
}

GraphNode* create_new_node(uint8_t weight, uint32_t level) {
    if (graph.current_node_index >= GRAPH_MAX_NODES) {
        return NULL;
    }
    
    GraphNode* node = &graph.nodes[graph.current_node_index];
    node->incoming_weight = weight;
    node->level = level;
    
    // Update weight-level index
    WeightLevelSlot* slot = &graph.index.slots[weight][level];
    
    // Grow array if needed
    if (slot->count >= slot->capacity) {
        uint32_t new_capacity = slot->capacity ? slot->capacity * 2 : INITIAL_NODES_PER_WEIGHT_LEVEL;
        uint32_t* new_indices = realloc(slot->indices, new_capacity * sizeof(uint32_t));
        if (!new_indices) return NULL;
        
        slot->indices = new_indices;
        slot->capacity = new_capacity;
    }
    
    // Add node to index
    slot->indices[slot->count++] = graph.current_node_index;
    
    // Update max level if needed
    if (level > graph.index.max_level) {
        graph.index.max_level = level;
    }
    
    graph.current_node_index++;
    return node;
}

bool is_graph_full(void) {
    return graph.current_node_index >= GRAPH_MAX_NODES;
}

uint32_t get_current_graph_node_index(void) {
    return graph.current_node_index - 1;
}

const uint32_t* get_nodes_by_weight_and_level(uint8_t weight, uint32_t level, uint32_t* count) {
    if (weight >= MAX_WEIGHT || level >= MAX_LEVELS) {
        *count = 0;
        return NULL;
    }
    
    WeightLevelSlot* slot = &graph.index.slots[weight][level];
    *count = slot->count;
    return slot->indices;
}

uint32_t get_max_level(void) {
    return graph.index.max_level;
}

void print_graph_node(const GraphNode *node, const uint8_t* block) {
    if (!node) {
        printf("NULL node\n");
        return;
    }
    printf("\n\nGraphNode @ %p:", (void*)node);
    printf("  id: %u", node->id);
    printf("  weight: %u", node->incoming_weight);
    printf("  level: %u", node->level);
    printf(",  saving: %d", node->saving_so_far);
    
    printf("\nParents: ");
    for (int i = 0; i < node->parent_count; i++) {
        printf("%u ", node->parents[i]);
    }
    
    printf("\nChildren: ");
    for (int i = 0; i < node->child_count; i++) {
        printf("%u ", node->children[i]);
    }
    
    printf("\nSequence: ");
    for (uint32_t i = node->compress_start_index; 
         i < node->compress_start_index + node->compress_sequence; 
         i++) {
        printf("0x%x ", block[i]);
    }
}


/*
// Create nodes with weight and level
GraphNode* node1 = create_new_node(5, 1);
GraphNode* node2 = create_new_node(5, 1);

// Add edges (works the same as before)
graph_add_edge(node1->id, node2->id);

// Process all nodes with weight=5 at level=1
uint32_t count;
const uint32_t* indices = get_nodes_by_weight_and_level(5, 1, &count);
for (uint32_t i = 0; i < count; i++) {
    GraphNode* node = graph_get_node(indices[i]);
    // Process node
}
*/