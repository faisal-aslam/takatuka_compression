// shortest_path_less_greedy.c
// shortest_path_less_greedy.c
//
// This file implements a "less greedy" shortest path finder in a graph-based compression system.
// The algorithm starts from a given level, picks the node with the highest cumulative savings,
// and moves upward through parent levels, preferring sequences already present in the frequency map.

#include "shortest_path_brute_force.h"
#include "shortest_path_common.h"
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>

#define MAX_BRUTE_FORCE 41

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

            /* Get frequency. For RLE or short sequences freq stays 1.
             * If the sequence is tracked in the map, use the map frequency otherwise fallback to 1.
             */
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

            /* Add best savings from parent level if available.
             * We use max_saving_node_ids[parent_level] (immediate best for that parent level)
             * and then take that node's cumulative best_savings (which was computed earlier
             * because we iterate levels bottom-up).
             */
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

            /* Update best node for this level. If none chosen yet (UINT32_MAX),
             * choose the first valid node; otherwise compare total savings.
             */
            if (best_savings_node_ids[level] == UINT32_MAX || node->best_savings > max_saving) {
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
    int idx = ++path_state->path_size[PATH_CURRENT]; // increment and use as index (first push from -1 to 0)
    path_state->path_stack[PATH_CURRENT][idx] = node->node_id;

    uint32_t freq = 1;
    if (!node->is_RLE && node->sequence_length > 1) {
        /* seq_freq_increment should return the new frequency after incrementing the stored count for this sequence.
         * We store that frequency in the path metadata.
         */
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
 * Search the given level range (from start_level .. level_in) for the node whose sequence exists
 * in the frequency map and yields the best trade-off (lowest cost, tie-breaker by savings, then length).
 *
 * @param level_in      The top level to search up to (inclusive).
 * @param block         Pointer to the data block being analyzed.
 * @param out_best_node_id Return - best saving node found that exists in seq map, or UINT32_MAX if none found.
 */
static void find_best_in_map(uint16_t level_in, const uint8_t *block, uint32_t *out_best_node_id) {
    *out_best_node_id = UINT32_MAX;

    /* We'll consider nodes starting from either level_in - MAX_BRUTE_FORCE (bounded) or 0. */
    uint16_t start_level = level_in > MAX_BRUTE_FORCE ? (level_in - MAX_BRUTE_FORCE) : 0;
    uint32_t start_id = get_level_start_id(start_level);
    uint32_t end_id = get_level_end_id(level_in); // end is exclusive (assumed)

    uint32_t best_cost = UINT32_MAX;   // use large initial value for comparisons
    uint32_t best_savings = 0;
    uint8_t best_length = 0;

    for (uint32_t id = start_id; id < end_id; id++) {
        GraphNode *node = get_graph_node(id);
        if (node->useless || node->sequence_length <= 1) continue;

        /* Reset freq/map_node_id for each node iteration to avoid leakage from previous iterations. */
        uint32_t freq = 0;
        uint32_t map_node_id = 0;

        if (node->is_RLE) {
            freq = 1;
        } else {
            /* seq_freq_get returns true and sets freq if sequence is in map. If not present, freq remains 0. */
            seq_freq_get(&block[node->offset], node->sequence_length, &freq, &map_node_id);
        }

        if (freq > 0) {
            uint32_t cost = calc_cost(node, freq);
            uint32_t savings = calc_savings(node, freq);

            /* Choose the best candidate using:
             *  1) lower cost,
             *  2) if equal cost, higher savings,
             *  3) if equal, longer sequence length.
             *
             * We also accept the first valid candidate when none chosen yet (*out_best_node_id == UINT32_MAX).
             */
            if (*out_best_node_id == UINT32_MAX ||
                cost < best_cost ||
                (cost == best_cost && savings > best_savings) ||
                (cost == best_cost && savings == best_savings && node->sequence_length > best_length)) {

                *out_best_node_id = node->node_id;
                best_cost = cost;
                best_savings = savings;
                best_length = node->sequence_length;
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
 *  3. For each node at starting_level:
 *       a) start a fresh current path and fresh sequence frequency map
 *       b) add the starting node to the current path
 *       c) move upward level-by-level:
 *           - try to find a node (within MAX_BRUTE_FORCE window) whose sequence already exists in the seq map
 *             and which yields a good saving (find_best_in_map).
 *           - if none found in the map, fall back to best_savings_node_ids[level].
 *           - if the chosen node is at a deeper level than `level`, build a sub-path (find_best_saving_path_to_a_node)
 *             and append that sub-path; otherwise append the chosen node directly via update_current_path().
 *           - stop when no candidate is available or root (node_id == 0) is reached.
 *
 * @param block           Pointer to the data block being analyzed.
 * @param starting_level  The level to start path construction from.
 * @param path_main       Output path structure where PATH_CURRENT will be constructed and then used to update best path.
 */
void find_best_saving_path(const uint8_t *block, uint16_t starting_level, Path *path_main) {
    if (starting_level >= graph.total_levels) return;

    /* Step 1: Find immediate best nodes (per-level). */
    compute_max_saving_node_ids(block, max_saving_node_ids);

    /* Step 2: Compute cumulative best savings nodes. */
    compute_best_savings_all(block, max_saving_node_ids, best_savings_node_ids);

#ifdef DEBUG
    fflush(stdout);
    visualize_graph(block);
    fflush(stdout);
#endif

    /* Initialize (global) path structure. Individual runs will reinitialize PATH_CURRENT. */
    path_init(path_main);

    /* Step 3: Iterate over every node in the starting level as a possible starting point. */
    uint32_t start_id_of_level = get_level_start_id(starting_level);
    uint32_t end_id_of_level = get_level_end_id(starting_level);

    for (uint32_t id = start_id_of_level; id < end_id_of_level; id++) {
        GraphNode *start_node = get_graph_node(id);
        if (start_node->useless) continue;

        /* Prepare a fresh current path and a fresh sequence-frequency map for this attempt. */
        path_init_current(path_main);
        init_seq_freq_map();

#ifdef DEBUG
        printf("At starting level %u selected ", start_node->node_level);
        print_graph_node(start_node);
#endif

        /* Add the starting node to the current path (also increments sequence map for that sequence). */
        update_current_path(start_node, block, path_main);

        /* Start climbing to parents from the starting node. get_parent_level() should return
         * a special out-of-range value (>= graph.total_levels) if there is no parent; the loop uses that.
         */
        uint16_t level = get_parent_level(start_node);

        while (level < graph.total_levels) {
            uint32_t chosen_node_id = UINT32_MAX;

            /* Search for a candidate in the seq map (within the allowed brute-force window). */
            find_best_in_map(level, block, &chosen_node_id);

            /* If nothing found in the seq map, fall back to the precomputed best node for that level. */
            if (chosen_node_id == UINT32_MAX) {
                chosen_node_id = best_savings_node_ids[level];
            }

            /* If still no candidate, there is nothing to add at this level — stop climbing. */
            if (chosen_node_id == UINT32_MAX) {
                fprintf(stderr, "No candidate found at level %u (map or best list); stopping climb.\n", level);
                abort();
            }

            /* Safely fetch the chosen node now that we know it's valid. */
            GraphNode *chosen_node = get_graph_node(chosen_node_id);

            /* If the chosen node is at a deeper (smaller index) level than 'level', we need to build
             * a sub-path from 'level' down to that chosen node, then append that sub-path to our current path.
             * Otherwise the chosen node lies exactly at 'level' and we can directly append it.
             */
            if (level > chosen_node->node_level) {
                Path path_state_intermediate;                

                /* Build the sub-path that reaches 'chosen_node_id' from 'level' */
                find_best_saving_path_to_a_node(block, level, chosen_node_id, &path_state_intermediate);

                /* Append the built best path (PATH_BEST of the intermediate) to our current path. */
                append_best_to_current(path_main, &path_state_intermediate);
            } else {
                /* chosen_node is exactly in this level - add it directly. */
                update_current_path(chosen_node, block, path_main);
            }

#ifdef DEBUG
            printf("At level %u selected ", chosen_node->node_level);
            print_graph_node(chosen_node);
            print_path(1, 1, block, path_main);
            seq_freq_map_print();
#endif

            /* Stop if we appended the root node (assumes node_id 0 is the root). */
            if (chosen_node->node_id == 0) break;

            /* Move up to the parent level of the chosen node and continue. */
            level = get_parent_level(chosen_node);
        }

        /* Step 5: Finalize: examine/update global best path using path_main (PATH_CURRENT). */
#ifdef DEBUG
        print_path(1, 1, block, path_main);
#endif
        update_best_path(block, path_main);

        /* iterate next starting node */
    }
}
