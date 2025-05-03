// src/second_pass/prune_logic.c

#include "prune_logic.h"
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // For memcpy
#include <stdlib.h>

#define SELECTION_SORT_THRESHOLD 32


static int level = 1;
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
    if (frequency == 0) {
        return seq_length*5;
    }
    double savings = (double)(pow(frequency+1, 1.8))*(pow(seq_length, 1.3));
    
    /**
     * Safety Cap:
     * - Ensures theoretical limits are respected
     */
    return (int32_t)MIN(savings, INT32_MAX);
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
        const int limit = (k < count) ? k : count; // Handle k > count case more clearly
        for (int i = 0; i < limit; i++) {
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
        const int limit = (k < count) ? k : count;
        for (int i = 0; i < limit; i++) {
            nodes[i]->isPruned = 0;
        }
    }
}

static inline void keep_top_k_by_seq_count(TreeNode** nodes, int count, int k) {
    if (count <= SELECTION_SORT_THRESHOLD) {
        // In-place partial selection sort for sequence count (ascending)
        int selected = 0;
        for (int i = 0; i < count && selected < k; i++) {
            // Skip already unpruned nodes (selected by other criteria)
            if (!nodes[i]->isPruned) continue;
            
            // Find minimum among remaining pruned nodes
            int min_idx = i;
            for (int j = i + 1; j < count; j++) {
                if (nodes[j]->isPruned && 
                    nodes[j]->compress_sequence_count < nodes[min_idx]->compress_sequence_count) {
                    min_idx = j;
                }
            }
            
            // Swap if needed
            if (i != min_idx) {
                TreeNode* temp = nodes[i];
                nodes[i] = nodes[min_idx];
                nodes[min_idx] = temp;
            }
            
            // Unprune the selected node
            nodes[i]->isPruned = 0;
            selected++;
        }
    } else {
        // Use qsort for larger arrays
        qsort(nodes, count, sizeof(TreeNode*), compare_by_seq_count);
        int selected = 0;
        for (int i = 0; i < count && selected < k; i++) {
            if (nodes[i]->isPruned) {
                nodes[i]->isPruned = 0;
                selected++;
            }
        }
    }
}


void apply_dual_beam_pruning(TreeNodePool *pool, int node_count,
                           const uint8_t *block, uint32_t block_index) {
    if (!pool || !pool->data || node_count <= 0) return;

    for (int weight = 0; weight <= SEQ_LENGTH_LIMIT; weight++) {
        // Collect nodes for this weight level
        TreeNode* nodes[MAX_NODE_PER_LEVEL];         
        
        int valid_count = 0;

        for (int i = 0; i < node_count; i++) {
            TreeNode *node = &pool->data[i];
            if (node->incoming_weight == weight) {
                if (valid_count >= MAX_NODE_PER_LEVEL) {
                    fprintf(stderr, "\n nodes exceeds the limit: level=%d, weight=%d, nodes=%d\n", level, weight, valid_count);
                    exit(EXIT_FAILURE);
                    break;
                }
                nodes[valid_count++] = node;
                node->isPruned = 1; // Default to pruned
            }
        }
        if (valid_count == 0) continue;

        // Apply both pruning criteria
        keep_top_k_by_savings(nodes, valid_count, BEAM_WIDTH_SAVINGS);

        if (BEAM_WIDTH_SEQ_COUNT > 0)  
            keep_top_k_by_seq_count(nodes, valid_count, BEAM_WIDTH_SEQ_COUNT);
  
        #ifdef DEBUG
        if (level == 11 && weight == 0) {
            printf("Stop here");
        }
        printf("\n Nodes before pruning ---> %d\n", valid_count);
        fprintf(stderr, "\n === Nodes for weight %d ===", weight);
        int prune = 0;
        for (int i = 0; i < valid_count; i++) {
            if (!nodes[i]->isPruned) {
                printf("\n=== Unproned ");
                print_tree_node(nodes[i], block, block_index);
            } else {
                prune ++;
                printf("\n=== Pruned ");
                print_tree_node(nodes[i], block, block_index);
            }
        }
        printf("\n Nodes AFTER pruning ---> %d\n", (valid_count-prune));
        #endif

    }
    level++;
    
    #ifdef DEBUG
    printf("\n\n ----------------- End of one level=%d, node_count=%d, max_node=%d ------------------- \n\n\n\n", level, node_count, MAX_NODE_PER_LEVEL);
    int unproned = 0;
    for (int loop=0; loop < node_count; loop++){
        TreeNode *node = &pool->data[loop];
        if (!node->isPruned) unproned++;
    }
    printf("\n\n ----------------- End of one level=%d, unproned=%d------------------- \n\n\n\n", level, unproned);
    #endif
}
