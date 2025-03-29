// src/second_pass/second_pass.h
#ifndef SECOND_PASS_H
#define SECOND_PASS_H

#include "../weighted_freq.h"

#define MAX_TREE_NODES (SEQ_LENGTH_LIMIT * (SEQ_LENGTH_LIMIT + 1) / 2)

typedef struct TreeNode {
    uint8_t binary_sequence[SEQ_LENGTH_LIMIT];
    long saving_so_far;
    int incoming_weight;
    int current_level;
} TreeNode;

void processSecondPass(const char* filename);
void processBlockSecondPass(uint8_t* block, long blockSize);
void pruneTreeNode(TreeNode* nodes, int* node_count);

#endif
