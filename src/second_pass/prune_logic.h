// src/second_pass/prune_logic.h

#ifndef PRUNE_LOGIC_H
#define PRUNE_LOGIC_H

#include "tree_node.h"
#include "binseq_hashmap.h"
#include "tree_node_pool.h"

// Configuration constants for pruning algorithm
#define BEAM_WIDTH_SAVINGS 5      // Keep top 5 paths by savings_so_far
#define BEAM_WIDTH_SEQ_COUNT 4    // Keep top 4 paths by compress_sequence_count
#define MAXIMUM_SAVING_CAP 500

/**
 * Calculates the potential savings from compressing a binary sequence
 * @param new_bin_seq The binary sequence to evaluate
 * @param seq_length Length of the sequence
 * @param map Hashmap containing frequency data of sequences
 * @return Calculated savings value (higher means more beneficial to compress)
 */
int32_t calculate_savings(const uint8_t* seq, uint16_t len, BinSeqMap* map);

/*
 * Applies dual-criteria pruning strategy:
 * - Keeps BEAM_WIDTH_SAVINGS paths with highest savings_so_far
 * - Keeps BEAM_WIDTH_SEQ_COUNT paths with lowest compress_sequence_count
 * @param pool Pool of tree nodes to process
 * @param node_count Total number of nodes
 * @param block The data block being processed
 * @param block_index Current position in the block
 * @param best_index Array tracking best node indices per weight level
 * @param best_saving Array tracking best saving values per weight level
 */
void apply_dual_beam_pruning(TreeNodePool* pool, int node_count, 
                           const uint8_t* block, uint32_t block_index,
                           int* best_index, int32_t* best_saving);

#endif // PRUNE_LOGIC_H
