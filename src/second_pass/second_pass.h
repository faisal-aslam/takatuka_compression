// src/second_pass/second_pass.h
#ifndef SECOND_PASS_H
#define SECOND_PASS_H
//#define DEBUG_PRINT 1

#include <limits.h>
#include "../weighted_freq.h"

#define COMPRESS_SEQUENCE_LENGTH 200 //We have to prune tree whenever we reach this length. 
#define MAX_TREE_NODES COMPRESS_SEQUENCE_LENGTH
#define SAVING_GAP SEQ_LENGTH_LIMIT*10

typedef struct TreeNode {
    uint8_t compress_sequence[COMPRESS_SEQUENCE_LENGTH];
    uint16_t compress_sequence_count;
    uint32_t saving_so_far;
    uint8_t incoming_weight;
    uint8_t isPruned;
} TreeNode;

void processSecondPass(const char* filename);
void processBlockSecondPass(uint8_t* block, long blockSize);

#endif
