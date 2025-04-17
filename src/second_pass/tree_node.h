#ifndef TREE_NODE_H
#define TREE_NODE_H

#include <stdint.h>
#include "binseq_hashmap.h"
#define COMPRESS_SEQUENCE_LENGTH 10000

typedef struct TreeNode {
    uint16_t compress_sequence[COMPRESS_SEQUENCE_LENGTH];
    uint16_t compress_sequence_count;
    int32_t saving_so_far; //it can be negative. 
    uint16_t incoming_weight;
    uint8_t isPruned;
    int headerOverhead;
    BinSeqMap *map;
} TreeNode;

#endif

