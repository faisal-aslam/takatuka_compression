// src/second_pass/second_pass.h
#ifndef SECOND_PASS_H
#define SECOND_PASS_H

#include "../weighted_freq.h"

#define MAX_TREE_NODES 6000000//(SEQ_LENGTH_LIMIT * (SEQ_LENGTH_LIMIT + 1) / 2)

typedef struct TreeNode {
    uint8_t compress_sequence;
    long saving_so_far;
    int incoming_weight;
    uint8_t isPruned;
    uint8_t sequence;
} TreeNode;

void processSecondPass(const char* filename);
void processBlockSecondPass(uint8_t* block, long blockSize);

#endif
