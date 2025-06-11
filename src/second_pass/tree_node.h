//tree_node.h
#ifndef TREE_NODE_H
#define TREE_NODE_H

#include "sequence_range.h"
#include "../constants.h"
#include "binseq_hashmap.h"

typedef struct TreeNode {
    uint16_t* compress_sequence;
    uint16_t compress_sequence_count;
    uint16_t compress_sequence_capacity;
    int32_t saving_so_far;
    uint16_t incoming_weight;
    uint8_t isPruned;
    uint32_t id; // to uniquely identifying a node.
    uint32_t parent_id; //to identify parent node.
    BinSeqMap *map;
} TreeNode;

// Function declarations
SequenceRange get_sequence_range(const TreeNode *node, const uint8_t *block);
TreeNode* create_tree_node(uint16_t initial_capacity);
void free_tree_node(TreeNode* node);
void init_tree_node(TreeNode* node, uint16_t initial_capacity);
int ensure_sequence_capacity(TreeNode* node, uint16_t needed_capacity);
void copy_tree_node_sequences(TreeNode *source, TreeNode *dest, uint16_t count);
void print_tree_node(const TreeNode *node, const uint8_t* block, uint32_t block_index);

#endif