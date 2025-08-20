#pragma once

#include "seq_freq_map.h"
#include "shortest_path.h"
#include "timer.h"
#include <math.h>

#define CHECK_INDEX(idx, label)                                                                                        \
    if ((idx) < 0 || (idx) >= MAX_LEVELS) {                                                                            \
        fprintf(stderr, "ERROR: Index %d out of bounds in %s (MAX_LEVELS = %d)\n", (idx), (label), MAX_LEVELS);        \
        abort();                                                                                                       \
    }

#ifdef DEBUG
static uint32_t best_count = 0;
#endif

/**
 * @brief Calculates the storage cost in bytes for adding a graph node to a path
 *
 *
 * @param node Pointer to graph node being evaluated
 * @param frequency Frequency count of this sequence in the data
 * @return uint32_t Storage cost in bytes (always >= 0)
 */
static inline uint32_t calc_cost(GraphNode *node, uint32_t frequency) {

    if (node->node_id == 0) return 0; // no cost for the root node.

    if (node->is_RLE) {
        return 1u; // prefer RLE the most.
    } else if (frequency > 1 && node->sequence_length > 1) {
        return 2u; // next perfer repeated sequences with freq more than 1.
    } else if (node->sequence_length == 1) {
        return 5u; // do not use sequences of length 1 unless needed.
    } else if (frequency == 1) {
        return 5u * node->sequence_length; // avoid making new sequences unless they are rewarded in future.
    }
    fprintf(stderr, "illegal cost calculation\n");
    abort();
    return UINT32_MAX;
}

/**
 * @brief Calculates the storage saving (integer version).
 *
 * @param node      Pointer to graph node being evaluated.
 * @param frequency Frequency count of this sequence in the data.
 * @return uint32_t Storage saving in bytes (integer approximation).
 */
static inline uint32_t calc_savings(GraphNode *node, uint32_t frequency) {
    // No savings for root node
    if (node->is_RLE) {
        return node->sequence_length * 2u;
    } else if (frequency > 1) {
        return node->sequence_length;
    }
    return 0;
}

/**
 * Initializes the path state for a new search.
 */
static inline void path_init(Path *path_state) {
    memset(path_state, 0, sizeof(Path));
    path_state->path_size[PATH_CURRENT] = -1;
    path_state->path_size[PATH_BEST] = -1;
    path_state->path_total_saving[PATH_CURRENT] = 0;
    path_state->path_total_saving[PATH_BEST] = 0;
    path_state->path_total_freq[PATH_BEST] = 0;
    path_state->path_total_freq[PATH_CURRENT] = 0;
    path_state->path_total_cost[PATH_CURRENT] = 0;
    path_state->path_total_cost[PATH_BEST] = UINT32_MAX;
}

/**
 * Initializes the path state for a new search.
 */
static inline void path_init_current(Path *path_state) {
    path_state->path_size[PATH_CURRENT] = -1;
    path_state->path_total_saving[PATH_CURRENT] = 0;
    path_state->path_total_freq[PATH_CURRENT] = 0;
    path_state->path_total_cost[PATH_CURRENT] = 0;
}
/**
 * Prints either the current path or the best path.
 * @param isCurrent If true, prints current path; otherwise prints best path
 * @param shouldPrintData If true, prints sequence details as well
 * @param block Pointer to input block (for sequence data)
 */
void print_path(uint8_t isCurrent, uint8_t shouldPrintData, const uint8_t *block, Path *path_state) {
    const int idx = isCurrent ? PATH_CURRENT : PATH_BEST;
    const int size = path_state->path_size[idx];

    if (size < 0) return; // No path

    CHECK_INDEX(size, "print_path");

    uint32_t total_saving = path_state->path_total_saving[idx];
    const uint32_t *stack = path_state->path_stack[idx];
    const uint32_t *freqs = path_state->path_freqs[idx];
    const uint32_t *per_node_savings = path_state->path_per_node_savings[idx];

    printf("\n=== %s PATH ===\n", isCurrent ? "CURRENT" : "BEST");
    printf("Path size = %d, Total saving = %u, Total cost=%u\n", size + 1, total_saving,
           path_state->path_total_cost[idx]);
    printf("Node chain (node_id, level):\n");

    for (int i = size; i >= 0; i--) {
        GraphNode *node = get_graph_node(stack[i]);
        if (!node) continue;
        printf("(%u,%u)", node->node_id, node->node_level);
        if (i > 0) printf(" -> ");
    }
    printf("\n");

    if (!shouldPrintData) return;

    printf("\nDetailed sequence info:\n");
    for (int i = size; i >= 0; i--) {
        GraphNode *node = get_graph_node(stack[i]);
        if (!node) continue;

        uint8_t len = node->sequence_length;
        uint32_t freq = freqs[i];
        uint32_t saving = per_node_savings[i];
        uint32_t cost_per_node = path_state->path_per_node_cost[idx][i];

        printf("\n -> ");
        if (node->is_RLE) {
            printf("RLE=YES ");
        }

        printf("| id=%u len=%u freq=%u saving=%u cost=%u | ", node->node_id, len, freq, saving, cost_per_node);

        print_node_sequence(node, block);

        if (i % 20 == 0) fflush(stdout);
    }

    printf("\n\n");
}

void free_path_state(Path *path_state) {
    // Only if path_state has dynamic allocations
    memset(path_state, 0, sizeof(Path));
}

void final_book_keeping(const uint8_t *block, Path *path_state) {
    init_seq_freq_map();
    GraphNode *node;
    const uint32_t *path = path_state->path_stack[PATH_BEST];
    int32_t path_len = path_state->path_size[PATH_BEST];
    if (path_len <= 0) return;

    // Pass 1: Count sequence frequencies
    for (int32_t i = 0; i < path_len; i++) {
        node = get_graph_node(path[i]);
        if (node->sequence_length > 1 && !node->is_RLE) {
            seq_freq_increment(&block[node->offset], node->sequence_length, node->node_id);
        }
    }

    // Pass 2: Store frequencies per node
    for (int32_t i = 0; i < path_len; i++) {
        node = get_graph_node(path[i]);
        if (node->sequence_length > 1 && !node->is_RLE) {
            uint32_t freq, node_id_unused;
            seq_freq_get(&block[node->offset], node->sequence_length, &freq, &node_id_unused);
            path_state->path_freqs[PATH_BEST][i] = freq;
        } else {
            path_state->path_freqs[PATH_BEST][i] = 1; // default for single-byte or RLE
        }
    }
}

void compute_max_saving_node_ids(const uint8_t *block, uint32_t *max_ids) {
    if (graph.total_levels < 2) return;
    //seq_freq_set_all(1, 3, 3);
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

        uint32_t max_saving = 0;
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

            uint32_t freq = 1, dummy_node, index;
            if (node->sequence_length > 1 && !node->is_RLE) {
                index = seq_freq_get_with_index(&block[node->offset], node->sequence_length, &freq, &dummy_node);
                //if (freq == 3) {
                  //  continue; //seen before.
                //} else if (index != UINT32_MAX) {
                   /// seq_freq_set_existing(index, 3, 1);
                  //  freq = 3;
                //}
            }           
            
            uint32_t saving = calc_savings(node, freq);

#ifdef DEBUG
            printf("  Node %u: seq_len = %u, lsis_RLE = %u, freq = %u, saving = %u\n", node->node_id,
                   node->sequence_length, node->is_RLE, freq, saving);
#endif

            if (saving > max_saving || best_node_id == UINT32_MAX) {
                max_saving = saving;
                best_node_id = node->node_id;
#ifdef DEBUG
                printf("    --> New best node: %u with saving %u\n", best_node_id, saving);
#endif
            }
        }

        max_ids[level] = best_node_id;
#ifdef DEBUG
        if (best_node_id == UINT32_MAX) {

            printf("[Level %u] No valid best node found.\n", level);
        }
#endif
    }
}

/**
 * @brief Roll back all sequence frequency increments for the given path.
 *
 * This is used when we don't want past increments to bias future best-path
 * calculations. It will decrement each sequence's frequency exactly once.
 *
 * @param path_index  PATH_CURRENT or PATH_BEST (depending on which you undo)
 * @param block       Pointer to the original data block
 */
static inline void rollback_path_freqs(int path_index, const uint8_t *block, Path *path_state) {
    int32_t size = path_state->path_size[path_index];
    if (size < 0) return; // No nodes in path

    const uint32_t *stack = path_state->path_stack[path_index];

    for (int32_t i = 0; i <= size; i++) {
        GraphNode *node = get_graph_node(stack[i]);
        if (!node) continue;

        // Only decrement for multi-byte non-RLE sequences
        if (node->sequence_length > 1 && !node->is_RLE) {
            // NOTE: seq_freq_decrement should safely handle cases where
            //       the sequence isn't found or freq is already 0.
            seq_freq_decrement(&block[node->offset], node->sequence_length);
        }
    }
}

/**
 * Updates the best path if the current path is better.
 */
static inline uint8_t update_best_path(const uint8_t *block, Path *path_state) {
    (void)block; // not used at the moment.
    uint8_t ret = 0;

    uint32_t saving_current = path_state->path_total_saving[PATH_CURRENT];
    uint32_t saving_best = path_state->path_total_saving[PATH_BEST];
    int32_t size_current = path_state->path_size[PATH_CURRENT];
    int32_t size_best = path_state->path_size[PATH_BEST];
    uint32_t freq_current = path_state->path_total_freq[PATH_CURRENT];
    uint32_t cost_current = path_state->path_total_cost[PATH_CURRENT];
    uint32_t cost_best = path_state->path_total_cost[PATH_BEST];

    if (size_best == -1 || cost_current < cost_best || (cost_current == cost_best && saving_current > saving_best) ||
        (cost_current == cost_best && saving_current == saving_best && size_current < size_best)) {

        // Undo increments from the current path so they don't bias future runs
        // rollback_path_freqs(PATH_CURRENT, block);

        int32_t size = size_current + 1;
        CHECK_INDEX(size - 1, "update_best_path copy");

        path_state->path_total_saving[PATH_BEST] = saving_current;
        path_state->path_size[PATH_BEST] = size_current;
        path_state->path_total_freq[PATH_BEST] = freq_current;
        path_state->path_total_cost[PATH_BEST] = cost_current;

        memcpy(path_state->path_stack[PATH_BEST], path_state->path_stack[PATH_CURRENT], size * sizeof(uint32_t));
        memcpy(path_state->path_per_node_savings[PATH_BEST], path_state->path_per_node_savings[PATH_CURRENT],
               size * sizeof(uint32_t));
        memcpy(path_state->path_freqs[PATH_BEST], path_state->path_freqs[PATH_CURRENT], size * sizeof(uint32_t));

        ret = 1;
#ifdef DEBUG
        best_count++;
        printf("[update_best_path] Best path updated %llu times\n", (unsigned long long)best_count);
#endif
    }

    return ret;
}