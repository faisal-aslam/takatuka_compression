// graph/graph.c

#include "graph.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "../map/binseq_hashmap.h"
#include "../map/node_map_pool.h"

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


static int merge_maps(GraphNode* g_node) {
    BinSeqMap* result = node_map_pool_get_next();    
    if (!result || !g_node) return -1;

    uint32_t current_level = g_node->level;

    binseq_map_init(result); // Initialize new map

    // 1. SPECIAL CASE: Single parent - shallow copy (O(1))
    if (g_node->parent_count == 1) {
        BinSeqMap* parent = node_map_pool_find(g_node->parents[0].map_index);
        if (parent) {
            // Copy only used entries (still O(1) with our fixed bounds)
            for (int i = 0; i < HASH_MAP_SIZE; i++) {
                if (parent->entries[i].used) {
                    binseq_map_put(result, 
                                 parent->entries[i].binary_sequence,
                                 parent->entries[i].length,
                                 parent->entries[i].frequency,
                                 current_level);
                }
            }
            return get_current_pool_index() - 1;
        }
    }

    // 2. LIMITED MERGE: Process first 8 entries from first 2 parents (O(1))
    int parents_to_process = MIN(2, g_node->parent_count);
    int entries_to_process = MIN(8, HASH_MAP_SIZE);
    
    for (int p = 0; p < parents_to_process; p++) {
        BinSeqMap* parent = node_map_pool_find(g_node->parents[p].map_index);
        if (!parent) continue;
        
        for (int i = 0; i < entries_to_process; i++) {
            Entry* src = &parent->entries[i];
            if (!src->used) continue;
            
            // Try insert with just 1 probe (O(1))
            binseq_map_put(result, 
                         src->binary_sequence,
                         src->length,
                         src->frequency,
                         current_level);
        }
    }
    
    return get_current_pool_index() - 1;
}

static inline int create_map(GraphNode* g_node, const uint8_t* block) {
    //step 1: merge graph of parent nodes and create new graph for this node.
    int map_index = merge_maps(g_node);
    if (g_node->compress_sequence_length == 1) {
        return map_index;
    }
    //step 2: Add current sequence in the newly created map.
    if(binseq_map_increment_frequency(node_map_pool_find(map_index),
                   &block[g_node->compress_start_index], g_node->compress_sequence_length, g_node->level)) {
        return map_index;
    }
    return -1;
}

bool graph_add_parent_edge(GraphNode* child_node, GraphNode* parent_node, const uint8_t* block) {
    // Check for valid node indices
    if (!child_node || !parent_node) return false;

#ifdef DEBUG
    printf("Adding edge: %u -> %u\n", child_node->id, parent_node->id);
    fflush(stdout);
#endif

    // Check if we can add more edges (within sequence length limit)
    if (child_node->parent_count >= SEQ_LENGTH_LIMIT) return false;

    // Correctly get a reference to the link and update it in-place
    ParentLink* link = &child_node->parents[child_node->parent_count++];
    link->parent_node_id = parent_node->id;
    link->map_index = create_map(child_node, block);
#ifdef DEBUG
    print_hashmap(node_map_pool_find(link->map_index));
#endif
    return true;
}


// Create a new node with given weight and level
GraphNode* create_new_node(uint8_t weight, uint32_t level) {
    if (graph.current_node_index >= GRAPH_MAX_NODES) {
        fprintf(stderr, "\n Nodes are more than max allowed\n");
        return NULL; // Graph full
    }
    if (level >= MAX_LEVELS) {
        fprintf(stderr, "\n Level are more than max allowed\n");
        return NULL; // Exceeds max levels
    }
    WeightLevelSlot* slot = &graph.index.slots[level][weight];
    if (slot->count >= SEQ_LENGTH_LIMIT) {
        fprintf(stderr, "\n Number of nodes on level are more than allowed \n");
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
         i < node->compress_start_index + node->compress_sequence_length; 
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