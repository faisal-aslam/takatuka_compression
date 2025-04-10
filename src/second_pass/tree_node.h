#ifndef TREE_NODE_H
#define TREE_NODE_H

#include <stdint.h>
#include "binseq_hashmap.h"

#define COMPRESS_SEQUENCE_LENGTH 10000 //We have to prune tree whenever we reach this length. 

typedef struct TreeNode {
    uint8_t compress_sequence[COMPRESS_SEQUENCE_LENGTH];
    uint16_t compress_sequence_count;
    uint32_t saving_so_far;
    uint8_t incoming_weight;
    uint8_t isPruned;
    BinSeqMap *map;
} TreeNode;

#endif

