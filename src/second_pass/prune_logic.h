// src/second_pass/prune_logic.h

#ifndef PRUNE_LOGIC_H
#define PRUNE_LOGIC_H

#include "tree_node.h"
#include "binseq_hashmap.h"
#include "tree_node_pool.h"

// Configuration constants
#define BEAM_WIDTH 5             
#define MIN_SEQUENCE_LENGTH 5    
#define POTENTIAL_THRESHOLD 0.6f 


int32_t calculate_savings(const uint8_t* new_bin_seq, uint16_t seq_length, BinSeqMap* map);
int is_potential_sequence(TreeNode* node, const uint8_t* block, uint32_t block_index);
void keep_top_nodes_by_weight(TreeNodePool* pool, int node_count, int weight, int keep);
void apply_hybrid_pruning(TreeNodePool* pool, int node_count, 
                         const uint8_t* block, uint32_t block_index,
                         int* best_index, int32_t* best_saving);

#endif // PRUNE_LOGIC_H