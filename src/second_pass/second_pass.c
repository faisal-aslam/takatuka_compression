// src/second_pass/second_pass.c
#include "second_pass.h"
#include <string.h>
#include <stdlib.h>

TreeNode node_pool_even[MAX_TREE_NODES];
int even_pool_count = 0;
TreeNode node_pool_odd[MAX_TREE_NODES];
int odd_pool_count = 0;


static inline void pruneTreeNodes(TreeNode* nodes, int* node_count) {
    // Basic pruning - keep only nodes with positive savings
/*    int kept = 0;
    for (int i = 0; i < *node_count; i++) {
        if (nodes[i].saving_so_far > 0) {
            if (kept != i) {
                memcpy(&nodes[kept], &nodes[i], sizeof(TreeNode));
            }
            kept++;
        }
    }
    *node_count = kept;
*/
}
static inline void initNode(TreeNode *node) {
	memset(node->compress_sequence, 0, sizeof(node->compress_sequence));
    node->compress_sequence_count = 0;
    node->saving_so_far = 0;
    node->incoming_weight = 1;
    node->isPruned = 0;
}

static inline void printNode(TreeNode *node) {
	printf("\n Node :");
	printf("saving_so_far=%ld, incoming_weight=%d, isPruned=%d", node->saving_so_far, node->incoming_weight, node->isPruned);
}

static inline void copyNodeCompressSeq(TreeNode *NodeA, TreeNode *NodeB, uint8_t bytes_to_copy) {
	// Copy all valid data from NodeA to NodeB
	memcpy(NodeB->compress_sequence, NodeA->compress_sequence, bytes_to_copy);

	// Update NodeB's count to match NodeA's
	NodeB->compress_sequence_count = bytes_to_copy;
}

static inline int createNodes(TreeNode *old_pool, TreeNode *new_pool, int old_node_count, uint8_t sequence) {
	int node_count = 0;
	for (int j=0; j < old_node_count; j++) { //loop through old nodes created previously.
		//for each old node.
		TreeNode oldNode = old_pool[j];
		//should create a total of oldNode.incoming_weight+1 new nodes.
		new_pool[node_count].incoming_weight = (oldNode.incoming_weight+1 < SEQ_LENGTH_LIMIT)? oldNode.incoming_weight+1 : SEQ_LENGTH_LIMIT-1;
		copyNodeCompressSeq(&oldNode, &new_pool[node_count], new_pool[node_count].compress_sequence_count);
		new_pool[node_count].compress_sequence[new_pool[node_count].compress_sequence_count++] = 1;
	    new_pool[node_count].saving_so_far = oldNode.saving_so_far;
	    printNode(&new_pool[node_count]);
		node_count++;
		for (int k=0; k < oldNode.incoming_weight; k++) { //rest of the oldNode.incoming_weight new nodes.
			new_pool[node_count].incoming_weight = 0;
			copyNodeCompressSeq(&oldNode, &new_pool[node_count], new_pool[node_count].compress_sequence_count-oldNode.incoming_weight-k);
			new_pool[node_count].compress_sequence[new_pool[node_count].compress_sequence_count++] = oldNode.incoming_weight-k;
			new_pool[node_count].saving_so_far = oldNode.saving_so_far + oldNode.incoming_weight-k;
			printNode(&new_pool[node_count]);
			node_count++;
		}	    
	}
	return node_count;
}

void processBlockSecondPass(uint8_t* block, long blockSize) {
	uint8_t isEven = 0;
	if (SEQ_LENGTH_LIMIT <= 1 || SEQ_LENGTH_LIMIT <= SEQ_LENGTH_START 
	|| blockSize <= 0 || block == NULL) { //some checks.
		return;
	}
	//create root
    node_pool_even[0].compress_sequence[0] = block[0];
    node_pool_even[0].compress_sequence_count = 1;
    node_pool_even[0].saving_so_far = 0;
    node_pool_even[0].incoming_weight = 1;
    printNode(&node_pool_even[0]);
    even_pool_count = 1; // there is one even pool node.
	
    for (long i = 1; i < blockSize; i++) {
    	printf("\n\n processing file byte number=%ld\n", i);

		if (isEven) {
			isEven = 0; //next time process oddnodes.
			printf("\n even 0x%x", block[i]);
			even_pool_count = createNodes(node_pool_odd, node_pool_even, odd_pool_count, block[i]);
		} else {
			isEven = 1;
			printf("\n odd 0x%x", block[i]);
			odd_pool_count = createNodes(node_pool_even, node_pool_odd, even_pool_count, block[i]);
		}
		
		/*        
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
        */
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
