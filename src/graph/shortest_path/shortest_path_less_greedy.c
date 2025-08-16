// shortest_path_less_greedy.c
//
// This file implements a "less greedy" shortest path finder in a graph-based compression system.
// The algorithm starts from a given level, picks the node with the highest cumulative savings,
// and moves upward through parent levels, preferring sequences already present in the frequency map.

#include "shortest_path_brute_force.h"
#include "shortest_path_common.h"
#include <limits.h>
#include <stdbool.h>

#define MAX_BRUTE_FORCE 3

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
static inline void update_current_path(GraphNode *node, const uint8_t *block, Path *path_state) {
    int idx = ++path_state->path_size[PATH_CURRENT]; // First push: from -1 to 0
    path_state->path_stack[PATH_CURRENT][idx] = node->node_id;

    uint32_t freq = 1;
    if (!node->is_RLE && node->sequence_length > 1) {
        freq = seq_freq_increment(&block[node->offset], node->sequence_length, node->node_id);
#ifdef DEBUG
        seq_freq_map_print();
#endif
    }

    uint32_t savings = calc_savings(node, freq);

    path_state->path_per_node_savings[PATH_CURRENT][idx] = savings;
    path_state->path_freqs[PATH_CURRENT][idx] = freq;
    path_state->path_total_saving[PATH_CURRENT] += savings;
    path_state->path_total_freq[PATH_CURRENT] += freq;
    path_state->path_total_cost[PATH_CURRENT] += calc_cost(node, freq);
}

/**
 * Search the given level for the node whose sequence exists in the frequency map
 * and yields the highest savings.
 *
 * @param level  The graph level to scan.
 * @param block  Pointer to the data block being analyzed.
 * @param out_best_node_id is return as the best saving node found in the map, or UINT32_MAX if none found.
 * @param out_level the leven of the node id found. Either it will be same as level_in or a parent of level_in.
 */
static void find_best_in_map(uint16_t level_in, const uint8_t *block, uint32_t *out_best_node_id, uint16_t *out_level) {
    uint32_t freq = 0, map_node_id = 0;
    uint32_t best_cost = 0;
    uint32_t best_savings = 0;
    uint8_t best_length = 0;
    *out_best_node_id = UINT32_MAX;
    for (uint16_t cur_level = level_in; cur_level >= level_in - MAX_BRUTE_FORCE; cur_level--) {
        uint32_t start_id = get_level_start_id(cur_level);
        uint32_t end_id = get_level_end_id(cur_level);
        for (uint32_t id = start_id; id < end_id; id++) {
            GraphNode *node = get_graph_node(id);
            if (node->useless || node->sequence_length <= 1) continue;
            if (node->is_RLE) {
                freq = 1;
            } else {
                seq_freq_get(&block[node->offset], node->sequence_length, &freq, &map_node_id);
            }
            if (freq > 0) {
                uint32_t cost = calc_cost(node, freq);
                uint32_t savings = calc_savings(node, freq);
                if (*out_best_node_id == UINT32_MAX || cost < best_cost ||
                    (cost == best_cost && savings > best_savings) ||
                    (cost == best_cost && savings == best_savings && node->sequence_length > best_length)) {
                    *out_best_node_id = node->node_id;
                    best_cost = cost;
                    best_savings = savings;
                    best_length = node->sequence_length;
                    *out_level = cur_level;
                }
            }
        }
    }
}

/**
 * Append PATH_BEST of src into PATH_CURRENT of dst.
 * Copies node IDs, per-node savings, frequencies, and updates totals.
 */
static inline void append_best_to_current(Path *dst, const Path *src) {
    int src_size = src->path_size[PATH_BEST];
    if (src_size < 0) return;

    for (int i = 0; i <= src_size; i++) {
        int dst_idx = ++dst->path_size[PATH_CURRENT];
        dst->path_stack[PATH_CURRENT][dst_idx] = src->path_stack[PATH_BEST][i];
        dst->path_per_node_savings[PATH_CURRENT][dst_idx] = src->path_per_node_savings[PATH_BEST][i];
        dst->path_freqs[PATH_CURRENT][dst_idx] = src->path_freqs[PATH_BEST][i];
    }

    dst->path_total_saving[PATH_CURRENT] += src->path_total_saving[PATH_BEST];
    dst->path_total_freq[PATH_CURRENT] += src->path_total_freq[PATH_BEST];
    dst->path_total_cost[PATH_CURRENT] += src->path_total_cost[PATH_BEST];
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
void find_best_saving_path(const uint8_t *block, uint16_t starting_level, Path *path_main) {
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
    path_init(path_main);

    // Step 3: Start from each node of the starting level
    uint32_t start_id_of_last_level = get_level_start_id(level);
    uint32_t end_id_of_last_level = get_level_end_id(level);
    for (uint32_t id = start_id_of_last_level; id < end_id_of_last_level; id++) {
        id = best_savings_node_ids[level]; // remove it later.
        GraphNode *node = get_graph_node(id);
        if (node->useless) continue;

        path_init_current(path_main);
        init_seq_freq_map();

#ifdef DEBUG
        printf("At starting level %u selected ", node->node_level);
        print_graph_node(node);
#endif
        update_current_path(node, block, path_main);

        // Step 4: Move upward through parents
        level = get_parent_level(node);
        while (level < graph.total_levels) {
            uint32_t chosen_node_id;
            uint16_t out_level;
            Path path_state_intermediate;
            find_best_in_map(level, block, &chosen_node_id, &out_level);
            if (chosen_node_id == UINT32_MAX) { // if unable to find best in map then use the best_saving_node.
                chosen_node_id =
                    best_savings_node_ids[level]; // this needs to be changed too to work with multiple levels.
            } else if (level > out_level) {
                find_best_saving_path_to_a_node(block, level, chosen_node_id, &path_state_intermediate);
            }

            node = get_graph_node(chosen_node_id);


            if (path_state_intermediate.path_size[PATH_BEST] > 0) {
                append_best_to_current(path_main, &path_state_intermediate);
            } else {
                update_current_path(node, block, path_main);
            }
#ifdef DEBUG
            printf("At level %u selected ", node->node_level);
            print_graph_node(node);
            print_path(1, 1, block, path_main);
#endif
            // Stop if root node reached (assumes node 0 is root)
            if (node->node_id == 0) break;

            level = get_parent_level(node);
        }

        // Step 5: Finalize and print path
#ifdef DEBUG
        print_path(1, 1, block, path_main);
#endif
        update_best_path(block, path_main);
        break; // remove it later.
    }
}
