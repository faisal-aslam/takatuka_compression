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

// Add a directed parent edge from 'child' node to 'parent' node
bool graph_add_parent_edge(GraphNode* child_node, GraphNode* parent_node) {
    // Check for valid node indices
    if (!child_node || !parent_node) return false;

#ifdef DEBUG
    printf("Adding edge: %u -> %u\n", child_node->id, parent_node->id);
    fflush(stdout);
#endif

    // Check if we can add more edges (within sequence length limit)
    if (child_node->parent_count >= SEQ_LENGTH_LIMIT) return false;

    // Add the link from child to parent (child --> parent)
    child_node->parents[child_node->parent_count++].parent_node_id = parent_node->id;
    child_node->parents[child_node->parent_count].link_id++;
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
    if (weight >= SEQ_LENGTH_LIMIT || level >= MAX_LEVELS) {
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

//returns the first node of the last level.
GraphNode* get_first_node_of_last_level(void) {
    uint32_t min_index = GRAPH_MAX_NODES;  // Larger than any valid index
    uint32_t max_level = graph.index.max_level;

    for (uint8_t weight = 0; weight < SEQ_LENGTH_LIMIT; ++weight) {
        WeightLevelSlot* slot = &graph.index.slots[max_level][weight];
        if (slot->count > 0) {
            uint32_t candidate = slot->indices[0]; // First node inserted for this weight
            if (candidate < min_index) {
                min_index = candidate;
            }
        }
    }

    if (min_index == GRAPH_MAX_NODES) {
        return NULL;  // No node found at last level
    }

    return &graph.nodes[min_index];
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
    //printf(",  saving: %d", node->saving_so_far);

    // Print parent nodes
    printf("\nParents: ");
    for (int i = 0; i < node->parent_count; i++) {
        printf("%u ", node->parents[i].parent_node_id);
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