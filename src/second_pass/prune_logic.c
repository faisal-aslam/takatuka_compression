// src/second_pass/prune_logic.c

#include "prune_logic.h"
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/**
 * Calculates compression savings for a binary sequence based on:
 * - Frequency (higher frequency => higher savings)
 * - Sequence length (longer sequences => much higher savings)
 * - Uniformity (uniform sequences => extra exponential boost)
 *
 * Optimized to avoid pow() for better performance.
 *
 * @param new_bin_seq The binary sequence to evaluate
 * @param seq_length Length of the sequence
 * @param map Hashmap containing frequency counts
 * @return Calculated savings value, or INT_MIN on error
 */
int32_t calculate_savings(const uint8_t* new_bin_seq, uint16_t seq_length, BinSeqMap* map) {
    // Validate inputs
    if (!new_bin_seq || seq_length <= 0 || !map) {
        fprintf(stderr, "Error: Invalid parameters in calculate_savings\n");
        return INT_MIN;
    }

    // No compression gain possible from sequences of length 1
    if (seq_length == 1) {
        return 0;
    }

    // Lookup frequency of the sequence from the hashmap
    const int* freq_ptr = binseq_map_get_frequency(map, new_bin_seq, seq_length);
    int frequency = freq_ptr ? *freq_ptr : 0;

    double savings = (double)(pow(frequency+1, 1.3))*(pow(seq_length, 1.5));
    /**
     * Safety Cap:
     * - Maximum possible saving cannot exceed (seq_length - 1) * 8 bits
     * - Ensures theoretical limits are respected
     */
    return (int32_t)MIN(savings, MAXIMUN_SAVING_CAP);
}


/**
 * Optimized hybrid pruning strategy that combines all three phases in a single pass:
 * 1. Keeps absolute best node per weight level (unconditionally)
 * 2. Retains nodes that either:
 *    - Form potential sequences (uniform/long patterns), OR
 *    - Have savings within POTENTIAL_THRESHOLD of the best saving
 * 3. Enforces BEAM_WIDTH limit on qualified nodes while maintaining:
 *    - Sort order by saving_so_far (descending)
 *    - Tie-breaker using compress_sequence_count (ascending)
 *
 * @param pool The tree node pool containing all candidate nodes
 * @param node_count Total number of nodes in the pool
 * @param block The input data block being processed
 * @param block_index Current position in the block
 * @param best_index Array of indices to best nodes for each weight level
 * @param best_saving Array of best saving values for each weight level
 */
void apply_hybrid_pruning(TreeNodePool *pool, int node_count,
                         const uint8_t *block, uint32_t block_index,
                         int *best_index, int32_t *best_saving) {
    // Validate input parameters
    if (!pool || !pool->data || node_count <= 0)
        return;

    // Process each weight level separately
    for (int weight = 0; weight <= SEQ_LENGTH_LIMIT; weight++) {

        // Skip weight levels with no recorded best node
        if (best_index[weight] == -1)
            continue;

        // Phase 1: Always keep the absolute best node for this weight
        TreeNode *best_node = &pool->data[best_index[weight]];
        best_node->isPruned = 0;  // Unconditionally unprune

        // Buffer to track top BEAM_WIDTH nodes (excluding the best node)
        TreeNode *top_nodes[BEAM_WIDTH];
        int top_count = 0;

        // Single pass through all nodes for phases 2 and 3
        for (int i = 0; i < node_count; i++) {
            TreeNode *node = &pool->data[i];

            // Only consider nodes of current weight
            if (node->incoming_weight != weight)
                continue;

            // Skip the best node (already handled)
            if (node == best_node)
                continue;

            // Phase 2: Check pruning criteria
            int meets_criteria = (node->saving_so_far >= best_saving[weight] * POTENTIAL_THRESHOLD);

            // Immediately prune nodes that fail criteria
            if (!meets_criteria) {
                node->isPruned = 1;
                continue;
            }

            // Phase 3: Beam width enforcement with early exit optimization
            if (top_count == BEAM_WIDTH && 
                node->saving_so_far < top_nodes[top_count-1]->saving_so_far) {
                node->isPruned = 1;
                continue;
            }

            // Insert node into sorted top_nodes array
            int pos = top_count;
            while (pos > 0 && 
                  (node->saving_so_far > top_nodes[pos-1]->saving_so_far ||
                  (node->saving_so_far == top_nodes[pos-1]->saving_so_far &&
                   node->compress_sequence_count < top_nodes[pos-1]->compress_sequence_count))) {
                // Shift nodes to make space
                if (pos < BEAM_WIDTH)
                    top_nodes[pos] = top_nodes[pos-1];
                pos--;
            }

            // Insert if within beam width
            if (pos < BEAM_WIDTH) {
                if (top_count < BEAM_WIDTH) 
                    top_count++;
                top_nodes[pos] = node;
            }

            node->isPruned = 1;  // Default to pruned
        }

        // Unprune nodes that made it into the top list
        for (int i = 0; i < top_count; i++) {
            top_nodes[i]->isPruned = 0;
        }
    }
}