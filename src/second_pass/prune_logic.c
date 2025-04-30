// src/second_pass/prune_logic.c

#include "prune_logic.h"
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // For memcpy
#include <stdlib.h>

#define SELECTION_SORT_THRESHOLD 32
#define MAX_NODE_PER_WEIGHT 256

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

// Comparison functions for qsort
static int compare_by_savings(const void* a, const void* b) {
    const TreeNode* node_a = *(const TreeNode**)a;
    const TreeNode* node_b = *(const TreeNode**)b;
    return (node_b->saving_so_far - node_a->saving_so_far); // Descending
}

static int compare_by_seq_count(const void* a, const void* b) {
    const TreeNode* node_a = *(const TreeNode**)a;
    const TreeNode* node_b = *(const TreeNode**)b;
    return (node_a->compress_sequence_count - node_b->compress_sequence_count); // Ascending
}

static inline void keep_top_k_by_savings(TreeNode** nodes, int count, int k) {
    if (count <= SELECTION_SORT_THRESHOLD) {
        // In-place partial selection sort for savings (descending)
        for (int i = 0; i < k && i < count; i++) {
            int max_idx = i;
            for (int j = i + 1; j < count; j++) {
                if (nodes[j]->saving_so_far > nodes[max_idx]->saving_so_far) {
                    max_idx = j;
                }
            }
            if (i != max_idx) {
                TreeNode* temp = nodes[i];
                nodes[i] = nodes[max_idx];
                nodes[max_idx] = temp;
            }
            nodes[i]->isPruned = 0;
            
            // Early exit if remaining elements can't affect top k
            if (count - i - 1 < k - (i+1)) break;
        }
    } else {
        // Use qsort for larger arrays
        qsort(nodes, count, sizeof(TreeNode*), compare_by_savings);
        for (int i = 0; i < k && i < count; i++) {
            nodes[i]->isPruned = 0;
        }
    }
}

static inline void keep_top_k_by_seq_count(TreeNode** nodes, int count, int k) {
    if (count <= SELECTION_SORT_THRESHOLD) {
        // In-place partial selection sort for sequence count (ascending)
        for (int i = 0; i < k && i < count; i++) {
            int min_idx = i;
            for (int j = i + 1; j < count; j++) {
                if (nodes[j]->compress_sequence_count < nodes[min_idx]->compress_sequence_count) {
                    min_idx = j;
                }
            }
            if (i != min_idx) {
                TreeNode* temp = nodes[i];
                nodes[i] = nodes[min_idx];
                nodes[min_idx] = temp;
            }
            nodes[i]->isPruned = 0;
            
            // Early exit if remaining elements can't affect top k
            if (count - i - 1 < k - (i+1)) break;
        }
    } else {
        // Use qsort for larger arrays
        qsort(nodes, count, sizeof(TreeNode*), compare_by_seq_count);
        for (int i = 0; i < k && i < count; i++) {
            nodes[i]->isPruned = 0;
        }
    }
}

void apply_dual_beam_pruning(TreeNodePool *pool, int node_count,
                           const uint8_t *block, uint32_t block_index) {
    if (!pool || !pool->data || node_count <= 0) return;

    for (int weight = 0; weight <= SEQ_LENGTH_LIMIT; weight++) {
        // Collect nodes for this weight level
        TreeNode* nodes[MAX_NODE_PER_WEIGHT]; 
        int valid_count = 0;

        for (int i = 0; i < node_count; i++) {
            TreeNode *node = &pool->data[i];
            if (node->incoming_weight == weight) {
                if (valid_count >= MAX_NODE_PER_WEIGHT) {
                    fprintf(stderr, "\n nodes exceeds the limit \n");
                    break;
                }
                nodes[valid_count++] = node;
                node->isPruned = 1; // Default to pruned
            }
        }

        if (valid_count == 0) continue;

        // Apply both pruning criteria
        keep_top_k_by_savings(nodes, valid_count, BEAM_WIDTH_SAVINGS);
        keep_top_k_by_seq_count(nodes, valid_count, BEAM_WIDTH_SEQ_COUNT);

    }
}
