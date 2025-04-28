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

    // Check if the sequence is made of identical bits (uniformity check)
    uint8_t first = new_bin_seq[0];
    int uniform = 1;
    for (int i = 1; i < seq_length; i++) {
        if (new_bin_seq[i] != first) {
            uniform = 0;
            break;
        }
    }

    double savings = 0.0;

    if (uniform) {
        /**
         * Uniform sequences (e.g., "0000..." or "1111...") compress extremely well.
         * Provide a heavy boost:
         * - Frequency^(1.4) approximated manually
         * - Sequence_length^(3.5) approximated manually
         *
         * Avoid using pow() by manual multiplications.
         */
        double freq_factor = (frequency + 1) * sqrt(frequency + 1); // ~ frequency^1.5
        double len_factor = (double)(seq_length * seq_length * seq_length) * sqrt(seq_length); // ~ seq_length^3.5

        savings = freq_factor * len_factor;

        // Extra bonus for being uniform and longer
        savings += seq_length * 10.0;
    } else {
        /**
         * Non-uniform sequences:
         * - Good compression if frequent and reasonably long
         * - Boost long patterns slightly more
         */
        double freq_factor = (double)frequency * pow((double)frequency, 0.1); // ~ frequency^1.1
        double len_factor = (double)((seq_length - 1) * (seq_length - 1)) * sqrt(seq_length - 1); // ~ (seq_length-1)^2.5

        savings = freq_factor * len_factor;

        // Small bonus for longer sequences
        savings += seq_length * 5.0;
    }

    /**
     * Safety Cap:
     * - Maximum possible saving cannot exceed (seq_length - 1) * 8 bits
     * - Ensures theoretical limits are respected
     */
    return (int32_t)MIN(savings, (seq_length - 1) * 8);
}



/**
 * Determines if a node represents a sequence worth preserving based on:
 * - Uniformity (all identical bits)
 * - Length approaching MIN_SEQUENCE_LENGTH threshold
 * @param node The tree node to evaluate
 * @param block The data block being processed
 * @param block_index Current position in the block
 * @return 1 if sequence is potentially compressible, 0 otherwise
 */
int is_potential_sequence(TreeNode* node, const uint8_t* block, uint32_t block_index) {
    // First check for uniform sequences (all bits identical)
    if (node->incoming_weight >= 2) {
        uint8_t first = block[block_index - node->incoming_weight + 1];
        int uniform = 1;
        for (uint32_t i = 1; i < node->incoming_weight; i++) {
            if (block[block_index - node->incoming_weight + 1 + i] != first) {
                uniform = 0;
                break;
            }
        }
        if (uniform) return 1;
    }
    
    // Then check if we're building a sequence that's approaching minimum length
    return (node->incoming_weight >= MIN_SEQUENCE_LENGTH - 1);
}

/**
 * Filters nodes to keep only the top performers by their saving_so_far value
 * Uses simple bubble sort since BEAM_WIDTH is typically small (5)
 * @param pool Pool of tree nodes to process
 * @param node_count Total number of nodes in the pool
 * @param weight The specific weight level to filter
 * @param keep Maximum number of nodes to retain
 */
void keep_top_nodes_by_weight(TreeNodePool* pool, int node_count, int weight, int keep) {
    // Validate inputs
    if (node_count <= 0 || !pool || !pool->data) return;
    
    // Allocate temporary array for nodes of the specified weight
    TreeNode** nodes = malloc(node_count * sizeof(TreeNode*));
    if (!nodes) return;
    
    // Collect all nodes matching the target weight
    int count = 0;
    for (int i = 0; i < node_count; i++) {
        if (pool->data[i].incoming_weight == weight && !pool->data[i].isPruned) {
            nodes[count++] = &pool->data[i];
        }
    }
    
    // If we're already under the keep limit, nothing to do
    if (count <= keep) {
        free(nodes);
        return;
    }
    
    // Simple bubble sort to order nodes by saving_so_far (descending)
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (nodes[j]->saving_so_far < nodes[j+1]->saving_so_far) {
                TreeNode* temp = nodes[j];
                nodes[j] = nodes[j+1];
                nodes[j+1] = temp;
            }
        }
    }
    
    // Mark all nodes beyond the 'keep' threshold as pruned
    for (int i = keep; i < count; i++) {
        nodes[i]->isPruned = 1;
    }
    
    free(nodes);
}

/**
 * Implements hybrid pruning strategy combining multiple criteria:
 * 1. Always keeps the absolute best node per weight level
 * 2. Retains nodes forming potential sequences (uniform or long)
 * 3. Preserves nodes with savings above POTENTIAL_THRESHOLD
 * 4. Enforces BEAM_WIDTH limit per weight level
 * @param pool Pool of tree nodes to process
 * @param node_count Total number of nodes
 * @param block The data block being processed
 * @param block_index Current position in the block
 * @param best_index Array of best node indices per weight level
 * @param best_saving Array of best saving values per weight level
 */
void apply_hybrid_pruning(TreeNodePool* pool, int node_count, 
    const uint8_t* block, uint32_t block_index,
    int* best_index, int32_t* best_saving) {
    // Validate inputs
    if (!pool || !pool->data || node_count <= 0) return;

    // Process each weight level separately
    for (int weight = 0; weight <= SEQ_LENGTH_LIMIT; weight++) {
        if (best_index[weight] != -1) {
            // 1. Always keep the absolute best node for this weight
            pool->data[best_index[weight]].isPruned = 0;

            // 2. Keep nodes that are building potential sequences
            // OR have savings above the threshold
            for (int i = 0; i < node_count; i++) {
                TreeNode* node = &pool->data[i];
                if (node->incoming_weight == weight) {
                    if (is_potential_sequence(node, block, block_index) || 
                        node->saving_so_far >= best_saving[weight] * POTENTIAL_THRESHOLD) {
                        node->isPruned = 0;
                    }
                }
            }
        }
        // 3. Enforce beam width limit for this weight level
        keep_top_nodes_by_weight(pool, node_count, weight, BEAM_WIDTH);
    }
}