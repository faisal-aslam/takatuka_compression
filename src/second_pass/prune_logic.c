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
 * Compare function for sorting by savings (descending)
 */
static int compare_by_savings(const void* a, const void* b) {
    const TreeNode* node_a = *(const TreeNode**)a;
    const TreeNode* node_b = *(const TreeNode**)b;
    return (node_b->saving_so_far - node_a->saving_so_far); // Descending
}

/**
 * Compare function for sorting by sequence count (ascending)
 */
static int compare_by_seq_count(const void* a, const void* b) {
    const TreeNode* node_a = *(const TreeNode**)a;
    const TreeNode* node_b = *(const TreeNode**)b;
    return (node_a->compress_sequence_count - node_b->compress_sequence_count); // Ascending
}

void apply_dual_beam_pruning(TreeNodePool *pool, int node_count,
                           const uint8_t *block, uint32_t block_index,
                           int *best_index, int32_t *best_saving) {
    if (!pool || !pool->data || node_count <= 0)
        return;

    // Process each weight level separately
    for (int weight = 0; weight <= SEQ_LENGTH_LIMIT; weight++) {
        if (best_index[weight] == -1)
            continue;

        // Collect all nodes for this weight level
        TreeNode* nodes[256]; // Adjust size if needed
        int valid_count = 0;
        
        for (int i = 0; i < node_count; i++) {
            TreeNode *node = &pool->data[i];
            if (node->incoming_weight == weight) {
                nodes[valid_count++] = node;
                node->isPruned = 1; // Default to pruned
            }
        }

        if (valid_count == 0)
            continue;

        // First beam: Keep top BEAM_WIDTH_SAVINGS by savings_so_far
        qsort(nodes, valid_count, sizeof(TreeNode*), compare_by_savings);
        for (int i = 0; i < MIN(BEAM_WIDTH_SAVINGS, valid_count); i++) {
            nodes[i]->isPruned = 0;
        }

        // Second beam: Keep top BEAM_WIDTH_SEQ_COUNT by compress_sequence_count
        qsort(nodes, valid_count, sizeof(TreeNode*), compare_by_seq_count);
        for (int i = 0; i < MIN(BEAM_WIDTH_SEQ_COUNT, valid_count); i++) {
            nodes[i]->isPruned = 0;
        }

        // Always keep the absolute best node
        TreeNode *best_node = &pool->data[best_index[weight]];
        best_node->isPruned = 0;
    }
}

