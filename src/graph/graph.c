// graph/graph.c

#include "graph.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include "../map/binseq_hashmap.h"
#include "../map/node_map_pool.h"

static ParentLink best_link_at_last_level;
static uint32_t best_savings_at_last_level;
static GraphNode* best_node_at_last_level;

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
    memset(&best_link_at_last_level, 0, sizeof(best_link_at_last_level));
    best_savings_at_last_level = 0;
    best_node_at_last_level = NULL;
}

/**
 * Finds and prints the best compression path through the graph.
 * Uses pre-tracked best node at last level for O(1) start.
 * 
 * @param block The input data block being compressed
 */
void find_and_print_best_path(const uint8_t* block) {
    uint32_t max_level = get_max_level();
    if (max_level == 0) {
        printf("Graph is empty\n");
        return;
    }
    uint8_t skip_next;

    // Array to store the best path node IDs
    uint8_t best_path[max_level + 1];
    int size = 0;
    // Start from the tracked best node at last level
    if (!best_node_at_last_level) {
        printf("\nNo best node tracked at last level\n");
        return;
    }

    GraphNode* current_node = best_node_at_last_level;
    uint32_t total_savings = best_savings_at_last_level;
    printf("\n node path start \n");
    // Store the path from last level to root
    while (current_node->level > 1) {
        if (skip_next == 0) {
            best_path[size++] = current_node->compress_sequence_length;
            printf("%u->", current_node->id);
            //print_graph_node(current_node, block);
            skip_next = current_node->compress_sequence_length-1;
        } else {
            skip_next--;
            printf("%u=>", current_node->id);
            //print_graph_node(current_node, block);
        }
        
        ParentLink* best_link = NULL;
        
        // For last level, use our pre-tracked best link
        if (current_node->level == max_level) {
            best_link = &best_link_at_last_level;
        } 
        // For other levels, find best parent normally
        else {
            uint32_t max_savings = 0;
            for (int i = 0; i < current_node->parent_link_count; i++) {
                if (current_node->parent_links[i].saving_so_far >= max_savings) {
                    max_savings = current_node->parent_links[i].saving_so_far;
                    best_link = &current_node->parent_links[i];                    
                }
            }
  
            if (!best_link) {
                printf("\n\nNo valid parents found for node %u at level %u\n", 
                      current_node->id, current_node->level);
                return;
            } 
        }
        
        current_node = graph_get_node(best_link->parent_node_id);
    }
    
    printf("\n node path ends \n");
    // Print the best path
    fflush(stdout);
    printf("\n\nBest Compression Path (Total Savings: %u):\n", total_savings);
    for (int i = 0; i < size; i++) {
        printf("\n\nAt %d Combination %u: ", i, best_path[i]);
        printf("\n");
    }
    fflush(stdout);
}
// Get a node by its index
GraphNode* graph_get_node(uint32_t index) {
    // Return NULL for out-of-bounds indices, otherwise return the node
    return (index < GRAPH_MAX_NODES) ? &graph.nodes[index] : NULL;
}


static int merge_maps(GraphNode* parent_node, uint32_t *parents_max_saving) {
    if (!parent_node) return -1;
    
    BinSeqMap* result = node_map_pool_get_next();    
    if (!result) return -1;

    *parents_max_saving = 0;
    uint32_t parent_level = parent_node->level;
    binseq_map_init(result); // Fresh map

    // Special case: single parent → shallow copy (O(1))
    if (parent_node->parent_link_count == 1) {        
        BinSeqMap* parent = node_map_pool_find(parent_node->parent_links[0].map_index);
        if (!parent) return -1;
        *result = *parent;
        return get_current_pool_index() - 1;
    }

    // Optimized limited merge: merge non-empty entries from parents
    int parents_to_process = MIN(16, parent_node->parent_link_count);
    for (int p = 0; p < parents_to_process; p++) {
        ParentLink parent_link = parent_node->parent_links[p];
        BinSeqMap* parent = node_map_pool_find(parent_link.map_index);
        if (!parent || parent->total_used == 0) continue;
        
        // Track maximum parent savings
        if (parent_link.saving_so_far > *parents_max_saving) {
            *parents_max_saving = parent_link.saving_so_far; 
        }

        // Process up to 8 entries per parent
        int entries_added = 0;
        for (int i = 0; i < HASH_MAP_SIZE && entries_added < 8; i++) {
            Entry* src = &parent->entries[i];
            if (!src->used) continue;

            // Get sequence data from repository
            const uint8_t* seq_data = seq_repo_get_data(&sequence_repo, src->sequence_id);
            uint16_t seq_len = seq_repo_get_length(&sequence_repo, src->sequence_id);
            // Insert into result map using repository-backed sequence
            binseq_map_put(result, seq_data, seq_len, src->frequency, parent_level+1);
            entries_added++;
        }
    }

    return get_current_pool_index() - 1;
}

static inline int create_map(GraphNode* parent_node, GraphNode* child_node, const uint8_t* block, uint32_t* total_savings) {
    //step 1: merge graph of parent nodes and create new graph for this node.
    int map_index = merge_maps(parent_node, total_savings);
    if (child_node->compress_sequence_length == 1) {
        return map_index;
    }
    //step 2: Add current sequence in the newly created map.
    if(binseq_map_increment_frequency(node_map_pool_find(map_index),
                   &block[child_node->compress_start_index], child_node->compress_sequence_length, child_node->level, total_savings)) {
        return map_index;
    }
    return -1;
}

bool graph_add_parent_edge(GraphNode* child_node, GraphNode* parent_node, const uint8_t* block) {
    // Check for valid node indices
    if (!child_node || !parent_node) return false;

    // Check if we can add more edges (within sequence length limit)
    if (child_node->parent_link_count >= SEQ_LENGTH_LIMIT) return false;

#ifdef DEBUG
    printf("Adding edge: %u -> %u\n", child_node->id, parent_node->id);
    fflush(stdout);
#endif

    // Get a reference to the new link and update it
    ParentLink* link = &child_node->parent_links[child_node->parent_link_count++];
    link->parent_node_id = parent_node->id;
    uint32_t savings = 0;
    link->map_index = create_map(parent_node, child_node, block, &savings);
    link->saving_so_far = savings;
    // Update best link tracking (only for last level)
    if (savings > best_savings_at_last_level) {
        best_savings_at_last_level = savings;
        best_link_at_last_level = *link;
        best_node_at_last_level = child_node;
    }
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
    printf("\nGraphNode @ %p:", (void*)node);
    printf("  id: %u", node->id);
    printf("  weight: %u", node->incoming_weight);
    printf("  level: %u", node->level);
    //printf(",  saving: %d", node->saving_so_far);

    // Print parent nodes
    printf("\nParents: ");
    for (int i = 0; i < node->parent_link_count; i++) {
        printf("%u ", node->parent_links[i].parent_node_id);
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