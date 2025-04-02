// src/second_pass/second_pass.h
#ifndef SECOND_PASS_H
#define SECOND_PASS_H

#include "../weighted_freq.h"

#define MAX_TREE_NODES 6000000//(SEQ_LENGTH_LIMIT * (SEQ_LENGTH_LIMIT + 1) / 2) //todo
#define COMPRESS_SEQUENCE_LENGTH SEQ_LENGTH_LIMIT*(SEQ_LENGTH_LIMIT+1) //todo

typedef struct TreeNode {
    uint8_t compress_sequence[COMPRESS_SEQUENCE_LENGTH];
    uint8_t compress_sequence_count;
    long saving_so_far;
    int incoming_weight;
    uint8_t isPruned;
} TreeNode;

void processSecondPass(const char* filename);
void processBlockSecondPass(uint8_t* block, long blockSize);

#endif
