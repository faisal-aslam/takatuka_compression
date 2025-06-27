#include "shortest_path.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

typedef struct {
    uint32_t node_id;
    uint64_t seen_sequences;
    int cost;
} StateEntry;

static StateEntry* state_table = NULL;
static uint32_t state_table_size = 0;
static uint32_t state_table_capacity = 0;

static inline uint64_t sequence_bitmask(uint8_t seq_id) {
    return (1ULL << seq_id);
}

static void ensure_state_capacity(uint32_t required_size) {
    if (required_size <= state_table_capacity) return;
    
    uint32_t new_capacity = state_table_capacity ? state_table_capacity * GROWTH_FACTOR : INITIAL_GRAPH_NODES;
    while (new_capacity < required_size) new_capacity *= GROWTH_FACTOR;
    
    StateEntry* new_table = realloc(state_table, new_capacity * sizeof(StateEntry));
    assert(new_table && "Failed to grow state table");
    state_table = new_table;
    state_table_capacity = new_capacity;
}

static int get_state_cost(uint32_t node_id, uint64_t seen_sequences) {
    for (uint32_t i = 0; i < state_table_size; i++) {
        if (state_table[i].node_id == node_id && 
            state_table[i].seen_sequences == seen_sequences) {
            return state_table[i].cost;
        }
    }
    return INT_MAX;
}

static void update_state(uint32_t node_id, uint64_t seen_sequences, int cost) {
    for (uint32_t i = 0; i < state_table_size; i++) {
        if (state_table[i].node_id == node_id && 
            state_table[i].seen_sequences == seen_sequences) {
            if (cost < state_table[i].cost) {
                state_table[i].cost = cost;
            }
            return;
        }
    }
    
    ensure_state_capacity(state_table_size + 1);
    state_table[state_table_size++] = (StateEntry){
        .node_id = node_id,
        .seen_sequences = seen_sequences,
        .cost = cost
    };
}

int find_shortest_path_with_sequences(uint32_t target_node_id) {
    if (state_table) {
        free(state_table);
        state_table = NULL;
        state_table_size = state_table_capacity = 0;
    }

    // Initialize with all leaf nodes (nodes with no parents)
    uint32_t node_count = get_graph_node_count();
    for (uint32_t i = 0; i < node_count; i++) {
        GraphNode* node = get_graph_node(i);
        if (node->parent_count == 0) {
            uint64_t initial_seen = sequence_bitmask(node->sequence_id);
            update_state(node->node_id, initial_seen, node->sequence_length + 1);
        }
    }

    // Process nodes in reverse level order (topological sort)
    uint16_t total_levels = get_total_levels();
    for (int level = total_levels; level >= 0; level--) {
        uint32_t start = get_level_start_id(level);
        uint32_t end = get_level_end_id(level);

        for (uint32_t node_idx = start; node_idx < end; node_idx++) {
            GraphNode* node = get_graph_node(node_idx);
            if (!node) continue;

            // For each state that reaches this node
            for (uint32_t state_idx = 0; state_idx < state_table_size; state_idx++) {
                if (state_table[state_idx].node_id != node->node_id) continue;

                uint64_t current_seen = state_table[state_idx].seen_sequences;
                int current_cost = state_table[state_idx].cost;

                // Process all parents
                for (uint8_t p = 0; p < node->parent_count; p++) {
                    ParentLink* link = &node->parent_link[p];
                    GraphNode* parent = get_graph_node(link->parent_id);
                    if (!parent) continue;

                    uint64_t new_seen = current_seen;
                    int edge_cost;
                    
                    if (current_seen & sequence_bitmask(parent->sequence_id)) {
                        edge_cost = 1;  // Sequence already seen
                    } else {
                        edge_cost = parent->sequence_length + 1;
                        new_seen |= sequence_bitmask(parent->sequence_id);
                    }

                    int new_cost = current_cost + edge_cost;
                    int existing_cost = get_state_cost(parent->node_id, new_seen);

                    if (new_cost < existing_cost) {
                        update_state(parent->node_id, new_seen, new_cost);
                    }
                }
            }
        }
    }

    // Find the minimal cost to reach the target node
    int min_cost = INT_MAX;
    for (uint32_t i = 0; i < state_table_size; i++) {
        if (state_table[i].node_id == target_node_id && 
            state_table[i].cost < min_cost) {
            min_cost = state_table[i].cost;
        }
    }

    return (min_cost == INT_MAX) ? -1 : min_cost;
}