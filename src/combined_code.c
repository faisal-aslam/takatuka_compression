// Combined C and H Files

// === FILE: /home/noman/takatuka/takatuka_compression/src/graph/shortest_path.h ===

#pragma once
#include <stdint.h>
#include "graph.h"
#include <limits.h>
#include <stdio.h>

#define PATH_CURRENT 0
#define PATH_BEST 1

extern uint32_t max_saving_node_ids[MAX_LEVELS];
extern uint32_t best_savings_node_ids[MAX_LEVELS];

typedef struct {
    uint32_t path_stack[2][MAX_LEVELS];   // 0 = current, 1 = best
    uint32_t path_freqs[2][MAX_LEVELS];   // frequencies per node
    uint32_t path_per_node_savings[2][MAX_LEVELS];  // per-node cost
    uint32_t path_size[2];                 // size of each path
    uint32_t path_total_saving[2];                  // total cost
    uint32_t path_total_freq[2];
} Path;

extern Path path_state;
extern long total_input_size;

// to find the path with maximum total savings from any leaf to the root node
void find_best_saving_path(const uint8_t* block, uint16_t starting_level);

void free_path_state();

void print_path(uint8_t isCurrent, uint8_t shouldPrintData, const uint8_t *block);

void final_book_keeping(const uint8_t* block);
// === FILE: /home/noman/takatuka/takatuka_compression/src/graph/shortest_path_common.h ===

#pragma once

#include "shortest_path.h"
#include "seq_freq_map.h"
#include "timer.h"
#include <math.h>




/**
 * @brief Calculates the storage saving (integer version).
 *
 * @param node      Pointer to graph node being evaluated.
 * @param frequency Frequency count of this sequence in the data.
 * @return uint32_t Storage saving in bytes (integer approximation).
 */
static inline uint32_t calc_savings(GraphNode *node, uint32_t frequency) {
    // No savings for root node
    if (node->node_id == 0) return 0;

    const uint8_t len = node->sequence_length;

    // Handle RLE case first (uses different saving model)
    if (node->is_RLE) {
        // RLE saving: pattern length + 1 byte for repeat count
        // Original was: (len_ratio)^3 in floating point
        // We'll approximate it here and clamp to avoid overflow.
        uint32_t ratio = (node->length_of_RLE / node->repeat_seq_length);
        uint64_t cubic = (uint64_t)ratio * ratio * ratio;
        return (cubic > UINT32_MAX) ? UINT32_MAX : (uint32_t)cubic;
    }

    // For sequences of length <= 1, no savings
    if (len <= 1 || frequency <= 1) {
        return 0;
    }

    
    uint64_t saving = (uint64_t)(frequency - 1) * len * len;
    return (saving > UINT32_MAX) ? UINT32_MAX : (uint32_t)saving;
}

#define CHECK_INDEX(idx, label)                                                                                        \
    if ((idx) < 0 || (idx) >= MAX_LEVELS) {                                                                            \
        fprintf(stderr, "ERROR: Index %d out of bounds in %s (MAX_LEVELS = %d)\n", (idx), (label), MAX_LEVELS);        \
        abort();                                                                                                       \
    }

/**
 * Initializes the path state for a new search.
 */
static inline void path_init() {
    memset(&path_state, 0, sizeof(Path));
    path_state.path_size[PATH_CURRENT] = UINT32_MAX;
    path_state.path_size[PATH_BEST] = UINT32_MAX;
    path_state.path_total_saving[PATH_CURRENT] = 0;
    path_state.path_total_saving[PATH_BEST] = UINT32_MAX;
    path_state.path_total_freq[PATH_BEST] = 0;
    path_state.path_total_freq[PATH_CURRENT] = 0;
}

/**
 * Initializes the path state for a new search.
 */
static inline void path_init_current() {    
    path_state.path_size[PATH_CURRENT] = -1;
    path_state.path_total_saving[PATH_CURRENT] = 0;
    path_state.path_total_freq[PATH_CURRENT] = 0;
}
/**
 * Prints either the current path or the best path.
 * @param isCurrent If true, prints current path; otherwise prints best path
 * @param shouldPrintData If true, prints sequence details as well
 * @param block Pointer to input block (for sequence data)
 */
void print_path(uint8_t isCurrent, uint8_t shouldPrintData, const uint8_t *block) {
    const int idx = isCurrent ? PATH_CURRENT : PATH_BEST;
    const int32_t size = path_state.path_size[idx];
    if (size < 0) return; // no path exist.
    CHECK_INDEX(size - 1, "print_path");

    const double total_saving = path_state.path_total_saving[idx];
    const uint32_t *stack = path_state.path_stack[idx];
    const uint32_t *freqs = path_state.path_freqs[idx];
    const double *per_node_savings = path_state.path_per_node_savings[idx];

    printf("\n=== %s PATH ===\n", isCurrent ? "CURRENT" : "BEST");
    printf("Path size = %d, Total saving = %.2lf, Total freq=%u \n", size + 1, total_saving,
           path_state.path_total_freq[idx]);
    printf("Node chain (node_id, level):\n");

    for (int32_t i = size; i >= 0; i--) {
        GraphNode *node = get_graph_node(stack[i]);
        if (!node) continue;
        printf("(%u,%u)", node->node_id, node->node_level);
        if (i > 0) printf(" -> ");
    }
    printf("\n");

    if (!shouldPrintData) return;

    printf("\nDetailed sequence info:\n");
    for (int32_t i = size; i >= 0; i--) {
        GraphNode *node = get_graph_node(stack[i]);
        if (!node) continue;

        const uint8_t len = node->sequence_length;
        const uint32_t freq = freqs[i];
        const double saving = per_node_savings[i];

        printf("\n -> ");
        if (node->is_RLE) {
            printf("RLE=YES ");
        }

        printf("| id=%u len=%u freq=%u saving=%.2f | ", node->node_id, len, freq, saving);

        print_node_sequence(node, block);

        if (i % 20 == 0) fflush(stdout);
    }

    printf("\n\n");
}


void free_path_state() {
    // Only if path_state has dynamic allocations
    memset(&path_state, 0, sizeof(Path));
}

void final_book_keeping(const uint8_t *block) {
    init_seq_freq_map();
    GraphNode *node;
    const uint32_t *path = path_state.path_stack[PATH_BEST];
    uint32_t path_len = path_state.path_size[PATH_BEST];

    // Pass 1: Count sequence frequencies
    for (uint32_t i = 0; i <= path_len; i++) {
        node = get_graph_node(path[i]);
        if (node->sequence_length > 1 && !node->is_RLE) {
            seq_freq_increment(&block[node->offset], node->sequence_length, node->node_id);
        }
    }

    // Pass 2: Store frequencies per node
    for (uint32_t i = 0; i <= path_len; i++) {
        node = get_graph_node(path[i]);
        if (node->sequence_length > 1 && !node->is_RLE) {
            uint32_t freq, node_id_unused;
            seq_freq_get(&block[node->offset], node->sequence_length, &freq, &node_id_unused);
            path_state.path_freqs[PATH_BEST][i] = freq;
        } else {
            path_state.path_freqs[PATH_BEST][i] = 1; // or other sentinel if needed
        }
    }
}

void compute_max_saving_node_ids(const uint8_t *block, uint32_t *max_ids) {
    if (graph.total_levels < 2) return;

    for (uint16_t level = graph.total_levels - 1; level != 0; level--) {
        uint32_t start = graph.first_node_of_level[level];
        uint32_t end = (level + 1 < graph.total_levels) ? graph.first_node_of_level[level + 1] : graph.size;

        if (start == UINT32_MAX || start >= end) {
            max_ids[level] = UINT32_MAX; // No nodes in this level
#ifdef DEBUG
            printf("[Level %u] Empty or invalid range (start=%u, end=%u), skipping.\n", level, start, end);
#endif
            continue;
        }

        double max_saving = -1.0;
        uint32_t best_node_id = UINT32_MAX;

#ifdef DEBUG
        printf("\n[Level %u] start=%u, end=%u\n", level, start, end);
#endif

        for (uint32_t i = start; i < end; i++) {
            GraphNode *node = &graph.nodes[i];
            if (node->useless) {
#ifdef DEBUG
                printf("  Node %u: useless, skipping\n", node->node_id);
#endif
                continue;
            }

            uint32_t freq = 1;
            if (node->sequence_length > 1 && !node->is_RLE) {
                uint32_t dummy_id;
                if (!seq_freq_get(&block[node->offset], node->sequence_length, &freq, &dummy_id)) {
                    freq = 1;
#ifdef DEBUG
                    printf("  Node %u: seq_freq not found, fallback freq = 1\n", node->node_id);
#endif
                }
            }

            double saving = calc_savings(node, freq);

#ifdef DEBUG
            printf("  Node %u: seq_len = %u, is_RLE = %u, freq = %u, saving = %0.2f\n",
                   node->node_id, node->sequence_length, node->is_RLE, freq, saving);
#endif

            if (saving > max_saving) {
                max_saving = saving;
                best_node_id = node->node_id;
#ifdef DEBUG
                printf("    --> New best node: %u with saving %0.2f\n", best_node_id, saving);
#endif
            }
        }

        max_ids[level] = best_node_id;

        if (best_node_id != UINT32_MAX) {
            GraphNode *node = get_graph_node(best_node_id);
#ifdef DEBUG
            printf("[Level %u] Best node selected: %u (saving %0.2f)\n", level, best_node_id, max_saving);
#endif
            if (0 && node->sequence_length > 1 && !node->is_RLE) {
                uint32_t freq, node_id;
                uint32_t index = seq_freq_get_with_index(&block[node->offset], node->sequence_length, &freq, &node_id);
                if (index != UINT32_MAX) {
                    seq_freq_set_existing(index, freq + 3, node_id);
#ifdef DEBUG
                    printf("  Boosted frequency of node %u to %u at index %u\n", node_id, freq + 3, index);
#endif
                }
#ifdef DEBUG
                else {
                    printf("  Could not boost frequency: node %u sequence not found in map.\n", node->node_id);
                }
#endif
            }
        } else {
#ifdef DEBUG
            printf("[Level %u] No valid best node found.\n", level);
#endif
        }
    }
}

/**
 * Updates the best path if the current path is better.
 */
static inline uint8_t update_best_path() {
    uint8_t ret = 0;

    uint32_t saving_current = path_state.path_total_saving[PATH_CURRENT];
    uint32_t saving_best = path_state.path_total_saving[PATH_BEST];
    uint32_t size_current = path_state.path_size[PATH_CURRENT];
    uint32_t size_best = path_state.path_size[PATH_BEST];
    uint32_t freq_current = path_state.path_total_freq[PATH_CURRENT];
    uint32_t freq_best = path_state.path_total_freq[PATH_BEST];

    if (size_best == UINT32_MAX || saving_current > saving_best || (saving_current == saving_best && size_current < size_best) ||
        (saving_current == saving_best && size_current == size_best && freq_current > freq_best)) {

        uint32_t size = size_current + 1;
        CHECK_INDEX(size - 1, "update_best_path copy");

        path_state.path_total_saving[PATH_BEST] = saving_current;
        path_state.path_size[PATH_BEST] = size_current;
        path_state.path_total_freq[PATH_BEST] = freq_current;

        memcpy(path_state.path_stack[PATH_BEST], path_state.path_stack[PATH_CURRENT], size * sizeof(uint32_t));
        memcpy(path_state.path_per_node_savings[PATH_BEST], path_state.path_per_node_savings[PATH_CURRENT], size * sizeof(uint32_t));
        memcpy(path_state.path_freqs[PATH_BEST], path_state.path_freqs[PATH_CURRENT], size * sizeof(uint32_t));

        ret = 1;
    }

    return ret;
}
// === FILE: /home/noman/takatuka/takatuka_compression/src/graph/shortest_path_less_greedy.c ===

// shortest_path_less_greedy.c
//
// This file implements a "less greedy" shortest path finder in a graph-based compression system.
// The algorithm starts from a given level, picks the node with the highest cumulative savings,
// and moves upward through parent levels, preferring sequences already present in the frequency map.

#include "shortest_path_common.h"
#include <limits.h>
#include <stdbool.h>

Path path_state;                          // Global path state tracker
uint32_t max_saving_node_ids[MAX_LEVELS]; // Best immediate-savings node per level
uint32_t best_savings_node_ids[MAX_LEVELS];
/**
 * Compute the best cumulative savings node for every level in the graph.
 * Cumulative savings = own savings + inherited savings from the best node in the parent level.
 *
 * @param block                  Pointer to the data block being analyzed.
 * @param max_saving_node_ids    Array mapping each level to the node with the maximum immediate savings.
 * @param best_savings_node_ids  Output array mapping each level to the node with the highest cumulative savings.
 */
void compute_best_savings_all(const uint8_t *block, const uint32_t *max_saving_node_ids,
                              uint32_t *best_savings_node_ids) {
    for (uint16_t level = 0; level < graph.total_levels; level++) {
        uint32_t start = get_level_start_id(level);
        uint32_t end = get_level_end_id(level);

        uint32_t max_saving = 0;
        best_savings_node_ids[level] = UINT32_MAX; // No valid node yet

#ifdef DEBUG
        printf("\n[Level %u] start=%u, end=%u\n", level, start, end);
#endif

        for (uint32_t i = start; i < end; i++) {
            GraphNode *node = &graph.nodes[i];

            if (node->useless) {
                node->best_savings = 0;
                continue;
            }

            // Get frequency (fallback to 1 if missing)
            uint32_t freq = 1, dummy_id = 0;
            if (!node->is_RLE && node->sequence_length > 1) {
                if (!seq_freq_get(&block[node->offset], node->sequence_length, &freq, &dummy_id)) {
                    freq = 1; // Fallback if sequence not found
#ifdef DEBUG
                    printf("  Node %u: seq_freq not found, fallback freq = 1\n", node->node_id);
#endif
                }
            }

            uint32_t own_saving = calc_savings(node, freq);
            uint32_t inherited_saving = 0;

            // Add best savings from parent level if available
            if (level > 0) {
                uint16_t parent_level = get_parent_level(node);
                if (parent_level < graph.total_levels && max_saving_node_ids[parent_level] != UINT32_MAX) {
                    GraphNode *parent = get_graph_node(max_saving_node_ids[parent_level]);                    
                    inherited_saving = parent->best_savings;
                    
                }
            }

            node->best_savings = own_saving + inherited_saving;

#ifdef DEBUG
            printf("  Node %u: freq=%u, own=%u, inherited=%u, total=%u\n", node->node_id, freq, own_saving,
                   inherited_saving, node->best_savings);
#endif

            // Update best node for this level
            if (node->best_savings > max_saving || best_savings_node_ids[level] == UINT32_MAX) {
                max_saving = node->best_savings;
                best_savings_node_ids[level] = node->node_id;
#ifdef DEBUG
                printf("    --> Node %u becomes best so far with total saving %u\n", node->node_id, node->best_savings);
#endif
            }
        }

#ifdef DEBUG
        if (best_savings_node_ids[level] != UINT32_MAX) {
            printf("[Level %u] Best node: %u with saving %u\n", level, best_savings_node_ids[level], max_saving);
        } else {
            printf("[Level %u] No valid best node found.\n", level);
        }
#endif
    }
}

/**
 * Append a node to the current path, update sequence frequency map, and
 * adjust cumulative path savings/frequencies.
 */
static inline void update_current_path(GraphNode *node, const uint8_t *block) {
    int idx = ++path_state.path_size[PATH_CURRENT];
    path_state.path_stack[PATH_CURRENT][idx] = node->node_id;
    uint32_t freq = 1;
    if (!node->is_RLE && node->sequence_length > 1) {
        freq = seq_freq_increment(&block[node->offset], node->sequence_length, node->node_id);
        seq_freq_map_print();
    }
    uint32_t savings = calc_savings(node, freq);

    path_state.path_per_node_savings[PATH_CURRENT][idx] = savings;
    path_state.path_freqs[PATH_CURRENT][idx] = freq;
    path_state.path_total_saving[PATH_CURRENT] += savings;
    path_state.path_total_freq[PATH_CURRENT] += freq;
}

/**
 * Search the given level for the node whose sequence exists in the frequency map
 * and yields the highest savings.
 *
 * @param level  The graph level to scan.
 * @param block  Pointer to the data block being analyzed.
 * @return Node ID of the best saving node found in the map, or UINT32_MAX if none found.
 */
static uint32_t find_best_in_map(uint16_t level, const uint8_t *block) {
    uint32_t start_id = get_level_start_id(level);
    uint32_t end_id = get_level_end_id(level);
    uint32_t freq = 0, map_node_id = 0;
    uint32_t best_savings = 0;
    uint32_t best_node_id = UINT32_MAX;

    for (uint32_t id = start_id; id < end_id; id++) {
        GraphNode *node = get_graph_node(id);
        if (node->useless || node->is_RLE || node->sequence_length <= 1) continue;
        if (seq_freq_get(&block[node->offset], node->sequence_length, &freq, &map_node_id) && freq > 0) {
            uint32_t savings = calc_savings(node, freq);
            if (savings >= best_savings) {
                best_node_id = node->node_id;
                best_savings = savings;
            }
        }
    }
    return best_node_id;
}

/**
 * Build the best savings path starting from a given level and moving upward.
 *
 * Steps:
 *  1. Compute per-level max savings nodes.
 *  2. Compute cumulative best savings for all nodes.
 *  3. Start at starting_level's best node and add it to the path.
 *  4. Move to parent level; if a node in the sequence map has higher savings, choose it.
 *  5. Continue until the root is reached (or no parent).
 *
 * @param block           Pointer to the data block being analyzed.
 * @param starting_level  The level to start path construction from.
 */
void find_best_saving_path(const uint8_t *block, uint16_t starting_level) {
    if (starting_level >= graph.total_levels) return;

    uint16_t level = starting_level;
    // Step 1: Find immediate best nodes
    compute_max_saving_node_ids(block, max_saving_node_ids);

    // Step 2: Compute cumulative best savings nodes
    compute_best_savings_all(block, max_saving_node_ids, best_savings_node_ids);

#ifdef DEBUG
    fflush(stdout);
    visualize_graph(block);
    fflush(stdout);
#endif

    // Initialize path and sequence frequency map
    path_init();

    // Step 3: Start from each node of the starting level
    uint32_t start_id_of_last_level = get_level_start_id(level);
    uint32_t end_id_of_last_level = get_level_end_id(level);
    for (uint32_t id = start_id_of_last_level; id < end_id_of_last_level; id++) {
        
        GraphNode *node = get_graph_node(id);
        if (node->useless) continue;

        path_init_current();
        init_seq_freq_map();
        
#ifdef DEBUG
        printf("At starting level %u selected ", node->node_level);
        print_graph_node(node);
#endif
        update_current_path(node, block);

        // Step 4: Move upward through parents
        level = get_parent_level(node);
        while (level < graph.total_levels) {
            uint32_t chosen_node_id = find_best_in_map(level, block);
            if (chosen_node_id == UINT32_MAX) { // if unable to find best in map then use the best_saving_node.
                chosen_node_id = best_savings_node_ids[level];
            }

            node = get_graph_node(chosen_node_id);
#ifdef DEBUG
            printf("At level %u selected ", node->node_level);
            print_graph_node(node);
#endif
            update_current_path(node, block);

            // Stop if root node reached (assumes node 0 is root)
            if (node->node_id == 0) break;

            level = get_parent_level(node);
        }

        // Step 5: Finalize and print path
        update_best_path();        
        
    }
    //do not need that. 
    //final_book_keeping(block);
    print_path(0, 1, block);

}
