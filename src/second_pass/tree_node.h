//tree_node.h
#ifndef TREE_NODE_H
#define TREE_NODE_H

#include <stdint.h>
#include "constants.h"
#include "binseq_hashmap.h"


typedef struct TreeNode {
    uint16_t* compress_sequence;  // It is dynamic allocated
    uint16_t compress_sequence_count;
    uint16_t compress_sequence_capacity;  // To track allocated size
    int32_t saving_so_far;
    uint16_t incoming_weight;
    uint8_t isPruned;
    BinSeqMap *map;
} TreeNode;

#endif

