// src/second_pass/prune_logic.h

#ifndef PRUNE_LOGIC_H
#define PRUNE_LOGIC_H


#include "binseq_hashmap.h"
#include "../constants.h"


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

#endif // PRUNE_LOGIC_H
