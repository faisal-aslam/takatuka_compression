#include "shortest_path.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

static PathState* state_table = NULL;
static uint32_t state_table_size = 0;
static uint32_t state_table_capacity = 0;

static inline uint8_t derive_sequence_id(const GraphNode* node) {
    // A simple hash that maps (offset, length) to 0–63
    return (node->offset ^ (node->sequence_length * 31)) % MAX_SEQUENCES;
}


// Returns a bitmask for a given sequence ID (used for tracking usage)
static inline uint64_t sequence_bitmask(uint8_t seq_id) {
    return (1ULL << seq_id);
}

// Ensures the state_table has capacity to store the required number of states
static void ensure_state_capacity(uint32_t required_size) {
    if (required_size <= state_table_capacity) return;

    uint32_t new_capacity = state_table_capacity ? state_table_capacity * GROWTH_FACTOR : INITIAL_GRAPH_NODES;
    while (new_capacity < required_size) new_capacity *= GROWTH_FACTOR;

    PathState* new_table = realloc(state_table, new_capacity * sizeof(PathState));
    assert(new_table && "Failed to grow state table");
    state_table = new_table;
    state_table_capacity = new_capacity;
}

// Frees memory associated with a single state (path array)
static void free_state(PathState* state) {
    if (state->path) {
        free(state->path);
    }
}

// Updates or inserts a state into the state_table
static void update_state(uint32_t node_id, uint64_t seen_sequences, 
                         int cost, uint32_t* path, uint32_t path_length) {
    for (uint32_t i = 0; i < state_table_size; i++) {
        // Check if a state for this node and seen set already exists
        if (state_table[i].node_id == node_id && 
            state_table[i].seen_sequences == seen_sequences) {
            if (cost < state_table[i].cost) {
                // Found better path → replace it
                free_state(&state_table[i]);
                state_table[i].cost = cost;
                state_table[i].path_length = path_length;
                state_table[i].path = path;
            } else {
                // Found worse path → discard
                free(path);
            }
            return;
        }
    }

    // Insert new state
    ensure_state_capacity(state_table_size + 1);
    state_table[state_table_size++] = (PathState){
        .node_id = node_id,
        .seen_sequences = seen_sequences,
        .cost = cost,
        .path = path,
        .path_length = path_length
    };
}

int find_shortest_path_to_sink(uint32_t **path, uint32_t *path_length) {
    *path = NULL;
    *path_length = 0;

    // Clean up any previous state
    if (state_table) {
        for (uint32_t i = 0; i < state_table_size; i++) {
            free_state(&state_table[i]);
        }
        free(state_table);
        state_table = NULL;
        state_table_size = state_table_capacity = 0;
    }

    // Start from the last level (source nodes)
    uint16_t last_level = get_last_level_index();
    uint32_t start = get_level_start_id(last_level);
    uint32_t end = get_level_end_id(last_level);

    // Initialize state_table with each source node
    for (uint32_t node_idx = start; node_idx < end; node_idx++) {
        GraphNode *node = get_graph_node(node_idx);
        if (!node) continue;

        // Initial "seen" mask and cost
        uint64_t initial_seen = 0;
        int initial_cost = 1;
        if (node->sequence_length > 1) {
            initial_seen = sequence_bitmask(derive_sequence_id(node));
            initial_cost = node->sequence_length + 1;
        }

        // Allocate initial path (just the node itself)
        uint32_t *initial_path = malloc(sizeof(uint32_t));
        initial_path[0] = node->node_id;

        // Store this initial state
        update_state(node->node_id, initial_seen, initial_cost, initial_path, 1);
    }

    // Process levels in reverse (from leaves to root)
    for (int level = last_level; level >= 0; level--) {
        // Snapshot the current state_table
        uint32_t current_states_count = state_table_size;
        PathState *current_states = malloc(current_states_count * sizeof(PathState));

        // Copy current states and their paths (deep copy)
        for (uint32_t i = 0; i < current_states_count; i++) {
            current_states[i] = state_table[i];
            current_states[i].path = malloc(state_table[i].path_length * sizeof(uint32_t));
            memcpy(current_states[i].path, state_table[i].path, state_table[i].path_length * sizeof(uint32_t));
        }

        // For each current state, expand backward to its parents
        for (uint32_t i = 0; i < current_states_count; i++) {
            PathState *current_state = &current_states[i];
            GraphNode *node = get_graph_node(current_state->node_id);
            if (!node) continue;

            uint16_t parent_count = get_parent_nodes_count(node);
            GraphNode *parent_nodes = get_parent_nodes(node);

            for (uint16_t p = 0; p < parent_count; p++) {
                GraphNode *parent = &parent_nodes[p];
                if (!parent) continue;

                // Copy seen-sequence bitmask and compute cost
                uint64_t new_seen = current_state->seen_sequences;
                int edge_cost = 1;

                if (node->sequence_length > 1) {
                    uint64_t seq_bit = sequence_bitmask(derive_sequence_id(node));
                    if ((current_state->seen_sequences & seq_bit) != 0) {
                        edge_cost = 1; // already seen → no penalty
                    } else {
                        edge_cost = node->sequence_length + 1; // new sequence → add cost
                        new_seen |= seq_bit;
                    }
                }

                // Create new path: parent + current path
                uint32_t *new_path = malloc((current_state->path_length + 1) * sizeof(uint32_t));
                new_path[0] = parent->node_id;
                memcpy(new_path + 1, current_state->path, current_state->path_length * sizeof(uint32_t));

                // Add or update state with this parent
                update_state(parent->node_id, new_seen,
                             current_state->cost + edge_cost,
                             new_path,
                             current_state->path_length + 1);
            }
        }

        // Cleanup temporary snapshot states
        for (uint32_t i = 0; i < current_states_count; i++) {
            free(current_states[i].path);
        }
        free(current_states);
    }

    // Find the best state that ends at root node (node_id == 0)
    PathState *best_state = NULL;
    int best_cost = INT_MAX;

    for (uint32_t i = 0; i < state_table_size; i++) {
        if (state_table[i].node_id == 0) {
            if (!best_state || state_table[i].cost < best_cost) {
                best_state = &state_table[i];
                best_cost = state_table[i].cost;
            }
        }
    }

    // Copy out the best path and return cost
    if (best_state) {
        *path_length = best_state->path_length;
        *path = malloc(*path_length * sizeof(uint32_t));
        memcpy(*path, best_state->path, *path_length * sizeof(uint32_t));

        int cost = best_state->cost;

        // Free all other states except best
        for (uint32_t i = 0; i < state_table_size; i++) {
            if (&state_table[i] != best_state) {
                free_state(&state_table[i]);
            }
        }
        free(state_table);
        state_table = NULL;
        state_table_size = state_table_capacity = 0;

        return cost;
    }

    // No valid path found
    for (uint32_t i = 0; i < state_table_size; i++) {
        free_state(&state_table[i]);
    }
    free(state_table);
    state_table = NULL;
    state_table_size = state_table_capacity = 0;

    return -1;
}
