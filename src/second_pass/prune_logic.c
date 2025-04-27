#include "prune_logic.h"
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

int32_t calculate_savings(const uint8_t* new_bin_seq, uint16_t seq_length, BinSeqMap* map) {
    if (!new_bin_seq || seq_length <= 0 || !map) {
        fprintf(stderr, "Error: Invalid parameters in calculate_savings\n");
        return INT_MIN;
    }

    if (seq_length == 1) {
        return 0;
    }

    const int* freq_ptr = binseq_map_get_frequency(map, new_bin_seq, seq_length);
    int frequency = freq_ptr ? *freq_ptr : 0;
  
    // Stronger boost for long uniform sequences
    uint8_t first = new_bin_seq[0];
    int uniform = 1;
    for (int i = 1; i < seq_length; i++) {
        if (new_bin_seq[i] != first) {
            uniform = 0;
            break;
        }
    }
    
    double savings;
    if (uniform) {
        // Exponential boost for uniform sequences
        savings = pow(frequency + 1, 1.5) * pow(seq_length, 3.0);
    } else {
        savings = pow(frequency, 1.2) * pow(seq_length - 1, 1.8);
    }
    
    return (int32_t)MIN(savings, (seq_length - 1) * 8);
}


int is_potential_sequence(TreeNode* node, const uint8_t* block, uint32_t block_index) {
    // Consider sequences as potential if:
    // 1. They're uniform, OR
    // 2. They're building a sequence longer than threshold
    
    // Uniform check
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
    
    // Building long sequence check
    return (node->incoming_weight >= MIN_SEQUENCE_LENGTH - 1);
}


void keep_top_nodes_by_weight(TreeNodePool* pool, int node_count, int weight, int keep) {
    if (node_count <= 0 || !pool || !pool->data) return;
    
    TreeNode** nodes = malloc(node_count * sizeof(TreeNode*));
    if (!nodes) return;
    
    int count = 0;
    
    for (int i = 0; i < node_count; i++) {
        if (pool->data[i].incoming_weight == weight && !pool->data[i].isPruned) {
            nodes[count++] = &pool->data[i];
        }
    }
    
    if (count <= keep) {
        free(nodes);
        return;
    }
    
    // Simple bubble sort
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (nodes[j]->saving_so_far < nodes[j+1]->saving_so_far) {
                TreeNode* temp = nodes[j];
                nodes[j] = nodes[j+1];
                nodes[j+1] = temp;
            }
        }
    }
    
    for (int i = keep; i < count; i++) {
        nodes[i]->isPruned = 1;
    }
    
    free(nodes);
}

void apply_hybrid_pruning(TreeNodePool* pool, int node_count, 
    const uint8_t* block, uint32_t block_index,
    int* best_index, int32_t* best_saving) {
    if (!pool || !pool->data || node_count <= 0) return;

    for (int weight = 0; weight <= SEQ_LENGTH_LIMIT; weight++) {
        if (best_index[weight] != -1) {
            // Always keep the absolute best
            pool->data[best_index[weight]].isPruned = 0;

            // Keep nodes that are either:
            // 1. Building potential sequences, OR
            // 2. Within threshold of best savings
            for (int i = 0; i < node_count; i++) {
                TreeNode* node = &pool->data[i];
                if (node->incoming_weight == weight) {
                if (is_potential_sequence(node, block, block_index) || node->saving_so_far >= best_saving[weight] * POTENTIAL_THRESHOLD) {
                    node->isPruned = 0;
                }
            }
            }
        }
        keep_top_nodes_by_weight(pool, node_count, weight, BEAM_WIDTH);
    }
}