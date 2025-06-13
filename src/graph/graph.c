// graph/graph.c

#include "graph.h"
#include <stdio.h>
#include <string.h>

// Global graph instance initialized to zero
Graph graph = {0};

// Initialize the graph structure
void graph_init(void) {
    //if (graph.initialized) return;  // Only initialize once

    // Clear the entire graph structure
    memset(&graph, 0, sizeof(graph));
    
    // Initialize node IDs
    for (uint32_t i = 0; i < GRAPH_MAX_NODES; i++) {
        graph.nodes[i].id = i;
    }
    memset(graph.weight_cache, 0, sizeof(graph.weight_cache));
    
    // Set initial max level and mark as initialized
    graph.index.max_level = 0;
    graph.initialized = true;
}

// Get a node by its index
GraphNode* graph_get_node(uint32_t index) {
    // Return NULL for out-of-bounds indices, otherwise return the node
    return (index < GRAPH_MAX_NODES) ? &graph.nodes[index] : NULL;
}

// Add a directed edge from 'from' node to 'to' node
bool graph_add_edge(uint32_t from, uint32_t to) {
    // Check for valid node indices
    if (from >= GRAPH_MAX_NODES || to >= GRAPH_MAX_NODES) return false;

#ifdef DEBUG
    printf("Adding edge: %u -> %u\n", from, to);
    fflush(stdout);
#endif

    GraphNode* src = &graph.nodes[from];
    GraphNode* dst = &graph.nodes[to];

    // Check if we can add more edges (within sequence length limit)
    if (src->child_count >= SEQ_LENGTH_LIMIT || dst->parent_count >= SEQ_LENGTH_LIMIT) return false;

    // Add the edge in both directions
    src->children[src->child_count++] = to;
    dst->parents[dst->parent_count++] = from;
    return true;
}

// Create a new node with given weight and level
GraphNode* create_new_node(uint8_t weight, uint32_t level) {
    if (graph.current_node_index >= GRAPH_MAX_NODES) {
        fprintf(stderr, " Nodes are more than max allowed\n");
        return NULL; // Graph full
    }
    if (level >= MAX_LEVELS) {
        fprintf(stderr, " Level are more than max allowed\n");
        return NULL; // Exceeds max levels
    }
    WeightLevelSlot* slot = &graph.index.slots[level][weight];
    if (slot->count >= SEQ_LENGTH_LIMIT) {
        fprintf(stderr, " Number of nodes on level are more than allowed \n");
        return NULL; // Invariant violated (per-level limit exceeded)
    }

    GraphNode* node = &graph.nodes[graph.current_node_index];
    node->incoming_weight = weight;
    node->level = level;

    slot->indices[slot->count++] = graph.current_node_index;
    graph.current_node_index++;

    if (level > graph.index.max_level) {
        graph.index.max_level = level;
    }
    return node;
}

// Check if the graph has reached maximum capacity
bool is_graph_full(void) {
    return graph.current_node_index >= GRAPH_MAX_NODES;
}

// Get the index of the most recently created node
uint32_t get_current_graph_node_index(void) {
    return graph.current_node_index - 1;
}

// Get all nodes with a specific weight and level
const uint32_t* get_nodes_by_weight_and_level(uint8_t weight, uint32_t level, uint32_t* count) {
    // Check for valid weight and level
    if (weight >= MAX_WEIGHT || level >= MAX_LEVELS) {
        *count = 0;
        return NULL;
    }
    WeightLevelSlot* slot = &graph.index.slots[level][weight];
    *count = slot->count;
    return slot->indices;
}


// Get the current maximum level in the graph
uint32_t get_max_level(void) {
    return graph.index.max_level;
}

// Print detailed information about a graph node
void print_graph_node(const GraphNode *node, const uint8_t* block) {
    if (!node) {
        printf("NULL node\n");
        return;
    }
    
    // Print basic node information
    printf("\n\nGraphNode @ %p:", (void*)node);
    printf("  id: %u", node->id);
    printf("  weight: %u", node->incoming_weight);
    printf("  level: %u", node->level);
    printf(",  saving: %d", node->saving_so_far);

    // Print parent nodes
    printf("\nParents: ");
    for (int i = 0; i < node->parent_count; i++) {
        printf("%u ", node->parents[i]);
    }

    // Print child nodes
    printf("\nChildren: ");
    for (int i = 0; i < node->child_count; i++) {
        printf("%u ", node->children[i]);
    }

    // Print the sequence this node represents
    printf("\nSequence: ");
    for (uint32_t i = node->compress_start_index; 
         i < node->compress_start_index + node->compress_sequence; 
         i++) {
        printf("0x%x ", block[i]);
        fflush(stdout);
    }
}


// Process all nodes with weight=5 at level=1
/*uint32_t count;
const uint32_t* indices = get_nodes_by_weight_and_level(5, 1, &count);
for (uint32_t i = 0; i < count; i++) {
    GraphNode* node = graph_get_node(indices[i]);
    // Process node
}
*/