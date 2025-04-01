// src/second_pass/second_pass.c
#include "second_pass.h"
#include <string.h>
#include <stdlib.h>

TreeNode node_pool[MAX_TREE_NODES];
int current_node_count = 0;

void pruneTreeNode(TreeNode* nodes, int* node_count) {
    // Basic pruning - keep only nodes with positive savings
    int kept = 0;
    for (int i = 0; i < *node_count; i++) {
        if (nodes[i].saving_so_far > 0) {
            if (kept != i) {
                memcpy(&nodes[kept], &nodes[i], sizeof(TreeNode));
            }
            kept++;
        }
    }
    *node_count = kept;
}

void processBlockSecondPass(uint8_t* block, long blockSize) {
	if (1) return;
    for (long i = 0; i < blockSize; i++) {
    	printf("\n\n processing file byte number=%ld\n", i);
        current_node_count = 0;
        
        // Create initial node for current byte
        if (current_node_count < MAX_TREE_NODES) {
            node_pool[current_node_count].binary_sequence[0] = block[i];
            node_pool[current_node_count].saving_so_far = 0;
            node_pool[current_node_count].incoming_weight = 1;
            node_pool[current_node_count].current_level = 1;
            current_node_count++;
        }

        // Process existing nodes
        for (int j = 0; j < current_node_count; j++) {
            TreeNode* current = &node_pool[j];
            
            // Check if we can extend this sequence
            if (current->current_level < SEQ_LENGTH_LIMIT && 
                (i + current->current_level) < blockSize) {
                
                // Create new extended node
                if (current_node_count < MAX_TREE_NODES) {
                    TreeNode* new_node = &node_pool[current_node_count];
                    memcpy(new_node->binary_sequence, current->binary_sequence, current->current_level);
                    new_node->binary_sequence[current->current_level] = block[i + current->current_level];
                    new_node->current_level = current->current_level + 1;
                    
                    // Check if this sequence exists in our hashmap
                    BinarySequence* found = lookupSequence(new_node->binary_sequence, new_node->current_level);
                    if (found) {
                        // Calculate savings based on group
                        int code_size = 0;
                        switch(found->group) {
                            case 1: code_size = GROUP1_CODE_SIZE; break;
                            case 2: code_size = GROUP2_CODE_SIZE; break;
                            case 3: code_size = GROUP3_CODE_SIZE; break;
                            case 4: code_size = GROUP4_CODE_SIZE; break;
                            default: code_size = new_node->current_level * 8; // No compression
                        }
                        new_node->saving_so_far = current->saving_so_far + 
                            (new_node->current_level * 8 - code_size);
                    } else {
                        // No compression for this sequence
                        new_node->saving_so_far = current->saving_so_far;
                    }
                    
                    new_node->incoming_weight = current->incoming_weight;
                    current_node_count++;
                }
            }
        }

        // Prune the tree nodes
        pruneTreeNode(node_pool, &current_node_count);

        // TODO: Here you would track the best compression path
        // and eventually output the compressed data
    }
}

void processSecondPass(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    uint8_t* block = (uint8_t*)malloc(BLOCK_SIZE);
    if (!block) {
        perror("Failed to allocate memory for block");
        fclose(file);
        return;
    }

    while (1) {
        long bytesRead = fread(block, 1, BLOCK_SIZE, file);
        if (bytesRead == 0) break;

        processBlockSecondPass(block, bytesRead);
    }

    free(block);
    fclose(file);
}
