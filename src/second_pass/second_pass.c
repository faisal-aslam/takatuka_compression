// src/second_pass/second_pass.c
#include "second_pass.h"
#include <string.h>
#include <stdlib.h>

TreeNode node_pool_even[MAX_TREE_NODES];
int even_pool_count = 0;
TreeNode node_pool_odd[MAX_TREE_NODES];
int odd_pool_count = 0;

/*
 * Prunes nodes by marking `isPruned = 1` for all but the one with the highest
 * `saving_so_far` in each `incoming_weight` group. If there is a tie,
 * one is pruned randomly.
 */
static inline void pruneTreeNodes(TreeNode* nodes, int node_count) {
    if (node_count <= 1) return;

    // Use a hash map alternative with direct indexing
    int best_index[SEQ_LENGTH_LIMIT*(SEQ_LENGTH_LIMIT+1)];  
    long best_saving[SEQ_LENGTH_LIMIT*(SEQ_LENGTH_LIMIT+1)];

    memset(best_index, -1, sizeof(best_index));
    memset(best_saving, 0x80, sizeof(best_saving));  // Set to LONG_MIN

    // Identify the best node for each incoming_weight
    for (int i = 0; i < node_count; i++) {
        int weight = nodes[i].incoming_weight;

        if (nodes[i].saving_so_far > best_saving[weight] || best_index[weight] == -1) {
            if (best_index[weight] != -1) nodes[best_index[weight]].isPruned = 1; // Prune previous best
            best_saving[weight] = nodes[i].saving_so_far;
            best_index[weight] = i;
            nodes[i].isPruned = 0; // Keep this node
        } else {
            nodes[i].isPruned = 1; // Prune this node
        }
    }
}


static inline void initNode(TreeNode *node) {
	memset(node->compress_sequence, 0, sizeof(node->compress_sequence));
    node->compress_sequence_count = 0;
    node->saving_so_far = 0;
    node->incoming_weight = 1;
    node->isPruned = 0;
}

static void printNode(TreeNode *node, uint8_t* block) {
	printf("\n Node :");
	printf("saving_so_far=%ld, incoming_weight=%d, isPruned=%d, compress_seq_count=%d", node->saving_so_far, node->incoming_weight, node->isPruned, node->compress_sequence_count);
	int block_index = 0;
	for (int i=0; i < node->compress_sequence_count; i++) {
		printf("\n\tNode compressed = %d", node->compress_sequence[i]);
		printf("\t { ");
		for (int j=0; j < node->compress_sequence[i]; j++) {
			printf("0x%x",block[block_index++]);
			if (j+1 < node->compress_sequence[i]) printf(", ");
		}
		printf(" }, ");
	}
}

static inline void copyNodeCompressSeq(TreeNode *NodeA, TreeNode *NodeB, uint8_t bytes_to_copy) {
	if(bytes_to_copy > 0) {
		// Copy all valid data from NodeA to NodeB
		memcpy(NodeB->compress_sequence, NodeA->compress_sequence, bytes_to_copy);

		// Update NodeB's count to match NodeA's
		NodeB->compress_sequence_count = bytes_to_copy;
	}
}

/**
* A binary sequence is valid if it is in our sequenceMap. 
*/
static inline int isValidSequence(int sequence_length, uint8_t* block, int currentLength) {
    if (currentLength < sequence_length) {
        fprintf(stderr, "Error: current length (%d) should be >= than sequence length (%d)\n", 
                currentLength, sequence_length);
        return 0;
    }

    uint8_t lastSequence[sequence_length];
    memcpy(lastSequence, block + (currentLength - sequence_length), sequence_length);

    return lookupSequence(lastSequence, sequence_length) != NULL;
}


/*
* The function works by switching between even and odd nodes. In one turn, one of them act as the old nodes and use to populate new nodes. 
* In the next turn the other becomes old nodes and use to populate new nodes. So on. 
* This function creates new nodes while ensuring that nodes with lower savings are pruned immediately.
* If a newly created node has better savings, it replaces the existing node in O(1) time.
*/
static int createNodes(TreeNode *old_pool, TreeNode *new_pool, int old_node_count, uint8_t* block, int currentLength) {
    int new_nodes_count = 0;
    
    // Track the best savings per incoming weight and their corresponding index in new_pool
    long best_saving[COMPRESS_SEQUENCE_LENGTH];
    int best_index[COMPRESS_SEQUENCE_LENGTH];

    memset(best_saving, 0x80, sizeof(best_saving));  // Initialize to LONG_MIN
    memset(best_index, -1, sizeof(best_index));      // Initialize indices to -1
    
#ifdef DEBUG_PRINT
    printf("\nold_node_count= %d", old_node_count);
#endif

    for (int j = 0; j < old_node_count; j++) {  // Loop through old nodes
        TreeNode oldNode = old_pool[j];
        if (oldNode.isPruned) continue;  // Skip pruned nodes
        
#ifdef DEBUG_PRINT        
        printf("\n******************** Processing OLD node ************");
        printNode(&oldNode, block);
        printf("\n******************************************************");
#endif
        // Generate the first type of new node (no new compression)
        int weight = (oldNode.incoming_weight + 1 < SEQ_LENGTH_LIMIT) ? oldNode.incoming_weight + 1 : SEQ_LENGTH_LIMIT - 1;
        long new_saving = oldNode.saving_so_far;

        if (new_saving > best_saving[weight]) {
            // Prune the previous best if it exists
            if (best_index[weight] != -1) {
                new_pool[best_index[weight]].isPruned = 1;
            }

            // Update best saving and index
            best_saving[weight] = new_saving;
            best_index[weight] = new_nodes_count;

            new_pool[new_nodes_count].incoming_weight = weight;
            copyNodeCompressSeq(&oldNode, &new_pool[new_nodes_count], oldNode.compress_sequence_count);
            new_pool[new_nodes_count].compress_sequence[new_pool[new_nodes_count].compress_sequence_count++] = 1;
            new_pool[new_nodes_count].saving_so_far = new_saving;
            new_pool[new_nodes_count].isPruned = 0;
#ifdef DEBUG_PRINT
            printNode(&new_pool[new_nodes_count], block);
#endif
            new_nodes_count++;
        }

        // Generate compressed nodes
        for (int k = 0; k < oldNode.incoming_weight; k++) {
            int compress_sequence_length = oldNode.incoming_weight + 1 - k;

            if (!isValidSequence(compress_sequence_length, block, currentLength)) continue; //skip invalid nodes.

            new_saving = oldNode.saving_so_far + oldNode.incoming_weight - k;
            if (new_saving <= best_saving[0]) continue;  // Prune before creation

            // Prune the previous best if it exists
            if (best_index[0] != -1) {
                new_pool[best_index[0]].isPruned = 1;
            }

            best_saving[0] = new_saving;
            best_index[0] = new_nodes_count;

            new_pool[new_nodes_count].incoming_weight = 0;
            int to_copy = oldNode.compress_sequence_count - oldNode.incoming_weight + k;
            copyNodeCompressSeq(&oldNode, &new_pool[new_nodes_count], to_copy);
            new_pool[new_nodes_count].compress_sequence[new_pool[new_nodes_count].compress_sequence_count++] = compress_sequence_length;
            new_pool[new_nodes_count].saving_so_far = new_saving;
            new_pool[new_nodes_count].isPruned = 0;
#ifdef DEBUG_PRINT
            printNode(&new_pool[new_nodes_count], block);
            printf("to copy = %d-%d+%d=%d", oldNode.compress_sequence_count, oldNode.incoming_weight, k, to_copy);
#endif
            new_nodes_count++;
        }
    }

    return new_nodes_count;
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
#ifdef DEBUG_PRINT
    printNode(&node_pool_even[0], block);
#endif

    even_pool_count = 1; // there is one even pool node.
	
    for (long i = 1; i < blockSize; i++) {
#ifdef DEBUG_PRINT
    	printf("\n\n processing file byte number=%ld\n", i);
#endif
		//if (i >= 4) break;
		if (isEven) {
			isEven = 0; //next time process oddnodes.
#ifdef DEBUG_PRINT			
			printf("\n\n --------------------------------- even 0x%x", block[i]);
#endif
			even_pool_count = createNodes(node_pool_odd, node_pool_even, odd_pool_count, block, i+1);
		} else {
			isEven = 1;
#ifdef DEBUG_PRINT			
			printf("\n\n---------------------------------- odd 0x%x", block[i]);
#endif
			odd_pool_count = createNodes(node_pool_even, node_pool_odd, even_pool_count, block, i+1);
		}
		
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
