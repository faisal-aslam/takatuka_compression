// src/second_pass/prune_logic.h

#ifndef PRUNE_LOGIC_H
#define PRUNE_LOGIC_H

#include "tree_node.h"
#include "binseq_hashmap.h"
#include "tree_node_pool.h"

// Configuration constants for pruning algorithm
#define BEAM_WIDTH_SAVINGS 7     // Keep top x paths by savings_so_far
#define BEAM_WIDTH_SEQ_COUNT 0   // Keep top y paths by compress_sequence_count
#define MAX_NODE_PER_LEVEL ((BEAM_WIDTH_SAVINGS*SEQ_LENGTH_LIMIT+(BEAM_WIDTH_SEQ_COUNT*SEQ_LENGTH_LIMIT))*SEQ_LENGTH_LIMIT)

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
 */
void apply_dual_beam_pruning(TreeNodePool* pool, int node_count, 
                           const uint8_t* block, uint32_t block_indexg);

#endif // PRUNE_LOGIC_H
