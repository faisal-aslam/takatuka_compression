//shortest_path_common.h

#pragma once

#include "seq_freq_map.h"
#include "shortest_path.h"
#include "timer.h"
#include <math.h>

static uint32_t best_count = 0;


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

    if (node->RLE_type) {
        return 1u; // prefer RLE the most.
    } else if (frequency > 1 && node->sequence_length > 1) {
        return 2u; // next perfer repeated sequences with freq more than 1.
    } else if (node->sequence_length == 1) {
        return 5u; // do not use sequences of length 1 unless needed.
    } else if (frequency == 1 && node->sequence_length > 1) {
        return node->sequence_length*5u; // avoid making new sequences unless they are rewarded in future.
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
    if (node->RLE_type) {
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


static inline void free_path_state(Path *path_state) {
    // Only if path_state has dynamic allocations
    memset(path_state, 0, sizeof(Path));
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
        if (node->sequence_length > 1 && !node->RLE_type) {
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
        best_count++;
#ifdef DEBUG        
        printf("[update_best_path] Best path updated %llu times\n", (unsigned long long)best_count);
#endif
    }

    return ret;
}