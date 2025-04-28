// src/second_pass/prune_logic.h

#ifndef PRUNE_LOGIC_H
#define PRUNE_LOGIC_H

#include "tree_node.h"
#include "binseq_hashmap.h"
#include "tree_node_pool.h"

// Configuration constants for pruning algorithm
#define BEAM_WIDTH 7             // Maximum number of nodes to keep per weight level
#define POTENTIAL_THRESHOLD 0.75f // Percentage of best saving to consider a node as potential

/**
 * Calculates the potential savings from compressing a binary sequence
 * @param new_bin_seq The binary sequence to evaluate
 * @param seq_length Length of the sequence
 * @param map Hashmap containing frequency data of sequences
 * @return Calculated savings value (higher means more beneficial to compress)
 */
int32_t calculate_savings(const uint8_t* seq, uint16_t len, BinSeqMap* map);

/*
 * Applies hybrid pruning strategy combining multiple criteria:
 * - Keeps absolute best node per weight level
 * - Retains nodes forming potential sequences
 * - Preserves nodes with savings above threshold
 * - Enforces beam width limit per weight level
 * @param pool Pool of tree nodes to process
 * @param node_count Total number of nodes
 * @param block The data block being processed
 * @param block_index Current position in the block
 * @param best_index Array tracking best node indices per weight level
 * @param best_saving Array tracking best saving values per weight level
 */
void apply_hybrid_pruning(TreeNodePool* pool, int node_count, 
                         const uint8_t* block, uint32_t block_index,
                         int* best_index, int32_t* best_saving);

#endif // PRUNE_LOGIC_H