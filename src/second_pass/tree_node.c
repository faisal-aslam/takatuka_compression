//----- tree_node.c

#include "tree_node.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

TreeNode* create_tree_node(uint16_t initial_capacity) {
    TreeNode* node = malloc(sizeof(TreeNode));
    if (!node) return NULL;
    
    init_tree_node(node, initial_capacity);
    return node;
}

void free_tree_node(TreeNode* node) {
    if (!node) return;
    
    if (node->compress_sequence) {
        free(node->compress_sequence);
        node->compress_sequence = NULL;
    }
    if (node->map) {
        binseq_map_free(node->map);
        node->map = NULL;
    }
}

void init_tree_node(TreeNode* node, uint16_t initial_capacity) {
    if (!node) return;
    
    memset(node, 0, sizeof(TreeNode));
    node->compress_sequence = malloc(initial_capacity * sizeof(uint16_t));
    if (!node->compress_sequence) {
        fprintf(stderr, "Failed to allocate sequence array\n");
        return;
    }
    node->compress_sequence_capacity = initial_capacity;
    node->compress_sequence_count = 0;
}

int ensure_sequence_capacity(TreeNode* node, uint16_t needed_capacity) {
    if (!node) return 0;
    
    if (node->compress_sequence_capacity >= needed_capacity) {
        return 1;
    }
    
    // Exponential growth strategy
    uint16_t new_capacity = node->compress_sequence_capacity == 0 ? 1 : node->compress_sequence_capacity * 2;
    while (new_capacity < needed_capacity) {
        new_capacity *= 2;
    }
    
    uint16_t* new_sequence = realloc(node->compress_sequence, new_capacity * sizeof(uint16_t));
    if (!new_sequence) {
        fprintf(stderr, "Failed to expand sequence capacity\n");
        return 0;
    }
    
    node->compress_sequence = new_sequence;
    node->compress_sequence_capacity = new_capacity;
    return 1;
}

void copy_tree_node_sequences(TreeNode *source, TreeNode *dest, uint16_t count) {
    if (!source || !dest || count == 0) {
        fprintf(stderr, "Invalid copy parameters\n");
        return;
    }
    
    if (!ensure_sequence_capacity(dest, count)) {
        return;
    }
    
    memcpy(dest->compress_sequence, source->compress_sequence, count * sizeof(uint16_t));
    dest->compress_sequence_count = count;
}

void print_tree_node(const TreeNode *node, const uint8_t* block, uint32_t block_index) {
    if (!node) {
        printf("[NULL NODE]\n");
        return;
    }
    
    printf("\n Node :");
    printf("saving_so_far=%d, incoming_weight=%d, isPruned=%d, compress_seq_count=%d", 
           node->saving_so_far,            
           node->incoming_weight, 
           node->isPruned, 
           node->compress_sequence_count);
    printf(", total Savings =%d", node->saving_so_far);
   
    int nodeCount = 0;
    for (int i=0; i < node->compress_sequence_count; i++) {
        printf("\n\tNode compressed = %d", node->compress_sequence[i]);
        printf("\t { ");
        for (int j=0; j < node->compress_sequence[i]; j++) {
            if (block && (nodeCount) < BLOCK_SIZE) {
                printf("0x%x", block[nodeCount]);
                nodeCount++;
            } else {
                printf("[INVALID]");
            }
            if (j+1 < node->compress_sequence[i]) printf(", ");
        }
        printf(" }, ");
    }
    
    if (node->map) {
        binseq_map_print(node->map);
    } else {
        printf("\nMap is NULL\n");
    }
}