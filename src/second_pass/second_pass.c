// src/second_pass/second_pass.c
#include "second_pass.h"
#include <string.h>
#include <stdlib.h>
#include "../write_in_file/write_in_file.h"

// Add these at the beginning of the file, after includes
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif


TreeNode node_pool_even[MAX_TREE_NODES] = {0};
TreeNode node_pool_odd[MAX_TREE_NODES] = {0};
int even_pool_count = 0;
int odd_pool_count = 0;

uint32_t best_saving_overall = 0;

static void printNode(TreeNode *node, uint8_t* block, uint32_t block_index) {
    if (!node) {
        printf("[NULL NODE]\n");
        return;
    }
    
    // Use direct values but add bounds checking
    printf("\n Node :");
    printf("saving_so_far=%u, incoming_weight=%d, isPruned=%d, compress_seq_count=%d", 
           node->saving_so_far, 
           node->incoming_weight, 
           node->isPruned, 
           (node->compress_sequence_count < COMPRESS_SEQUENCE_LENGTH) ? node->compress_sequence_count : 0);
             
    // Bounds checking
    if (node->compress_sequence_count > COMPRESS_SEQUENCE_LENGTH) {
        printf("\n\t[ERROR: Invalid compress_sequence_count]");
        return;
    }
    
    for (int i=0; i < node->compress_sequence_count; i++) {
        printf("\n\tNode compressed = %d", node->compress_sequence[i]);
        printf("\t { ");
        for (int j=0; j < node->compress_sequence[i]; j++) {
            if (block && (block_index + j) < BLOCK_SIZE) {
                printf("0x%x", block[block_index + j]);
            } else {
                printf("[INVALID]");
            }
            if (j+1 < node->compress_sequence[i]) printf(", ");
        }
        printf(" }, ");
    }
}

static inline void copyNodeCompressSeq(TreeNode *NodeA, TreeNode *NodeB, uint8_t bytes_to_copy) {
    if (!NodeA || !NodeB || bytes_to_copy == 0 || bytes_to_copy > COMPRESS_SEQUENCE_LENGTH) {
        return;
    }
    
    // Copy all valid data from NodeA to NodeB
    memcpy(NodeB->compress_sequence, NodeA->compress_sequence, bytes_to_copy);
    
    // Update NodeB's count to match NodeA's
    NodeB->compress_sequence_count = bytes_to_copy;
}



/**
* A binary sequence is valid if it is in our sequenceMap. 
*/
static inline BinarySequence* isValidSequence(uint16_t sequence_length, uint8_t* block, uint32_t block_index) {
    if (sequence_length < SEQ_LENGTH_START || sequence_length > SEQ_LENGTH_LIMIT || block_index < sequence_length) {
        fprintf(stderr, "\n\n\n *************** Error: current length (%u) should be > than sequence length (%u)\n", 
                block_index, sequence_length);
        return NULL;
    }

    uint8_t lastSequence[sequence_length];
    memcpy(lastSequence, block + (block_index - sequence_length), sequence_length);

    return lookupSequence(lastSequence, sequence_length);
}

/**
 * Creates new tree nodes while enforcing pruning strategy
 * 
 * @param old_pool         Source node pool
 * @param new_pool         Destination node pool
 * @param old_node_count   Valid nodes in old_pool
 * @param block            Current data block
 * @param block_index      Position in block
 * @return                 Number of nodes created in new_pool
 */
 static int createNodes(TreeNode *old_pool, TreeNode *new_pool, int old_node_count,
                      uint8_t* block, uint32_t block_index) {
    // Validate inputs
    if (!old_pool || !new_pool || !block || old_node_count < 0) {
        return 0;
    }

    // Initialize tracking arrays  
	long best_saving[COMPRESS_SEQUENCE_LENGTH] = {0};
	int best_index[COMPRESS_SEQUENCE_LENGTH] = {-1};

	for (int i = 0; i < COMPRESS_SEQUENCE_LENGTH; i++) {
		best_saving[i] = 0;
		best_index[i] = -1;
	}
 


    int new_nodes_count = 0;
    for (int j = 0; j < old_node_count; j++) {
        
        TreeNode oldNode = old_pool[j];  
        
        // Skip pruned nodes - now safe to check isPruned
        if (oldNode.isPruned) {
            continue;
        }

        // Rest of the function remains the same...
        // =====================================================
        // 1. Create non-compressed node (sequence extension)
        // =====================================================
        {
            TreeNode newNode = {0}; // Fully zero-initialized
            
            // Calculate new weight (capped at SEQ_LENGTH_LIMIT-1)
            uint8_t new_weight = oldNode.incoming_weight + 1;
            if (new_weight >= SEQ_LENGTH_LIMIT) {
                new_weight = SEQ_LENGTH_LIMIT - 1;
            }

            uint32_t new_saving = oldNode.saving_so_far;

            // Only keep if better than existing nodes with same weight
            if (best_index[new_weight] == -1 || new_saving > best_saving[new_weight]) {
                // Prune previous best if exists
                if (best_index[new_weight] != -1) {
                    new_pool[best_index[new_weight]].isPruned = 1;
                }

                // Update tracking
                best_saving[new_weight] = new_saving;
                best_index[new_weight] = new_nodes_count;

                // Configure new node
                newNode.incoming_weight = new_weight;
                newNode.saving_so_far = new_saving;
                newNode.isPruned = 0;

                // Copy compression history safely
                copyNodeCompressSeq(&oldNode, &newNode, oldNode.compress_sequence_count);

                // Add new uncompressed byte (length=1)
                if (newNode.compress_sequence_count < COMPRESS_SEQUENCE_LENGTH) {
                    newNode.compress_sequence[newNode.compress_sequence_count++] = 1;
                } else {
                    fprintf(stderr, "\n\n\n Error: newNode.compress_sequence_count(%d) >= COMPRESS_SEQUENCE_LENGTH (%d)\n", 
                        newNode.compress_sequence_count, COMPRESS_SEQUENCE_LENGTH);
                }

                // Store in pool
                memcpy(&new_pool[new_nodes_count], &newNode, sizeof(TreeNode));
                new_nodes_count++;
            }
        }

        // =====================================================
        // 2. Create compressed nodes (if valid sequences exist)
        // =====================================================
        for (int k = 0; k < oldNode.incoming_weight; k++) {
            uint16_t seq_len = oldNode.incoming_weight + 1 - k;
            
            // Validate sequence exists
            BinarySequence* bin_seq = isValidSequence(seq_len, block, block_index);
            if (!bin_seq || bin_seq->group > TOTAL_GROUPS) {
                continue;
            }

            // Calculate new savings (in bits)
            uint32_t new_saving = oldNode.saving_so_far + 
                                (bin_seq->length * 8 - (groupCodeSize(bin_seq->group)+groupOverHead(bin_seq->group)));

            // Skip if not better than current best
            if (new_saving <= best_saving[0]) { //In this case our weight will be always zero
                continue;
            }

            TreeNode newNode = {0}; // Fully zero-initialized

            // Prune previous best
            if (best_index[0] != -1) {
                new_pool[best_index[0]].isPruned = 1;
            }

            // Update tracking
            best_saving[0] = new_saving;
            best_index[0] = new_nodes_count;
            if (new_saving > best_saving_overall) {
                best_saving_overall = new_saving;
            }

            // Configure new node
            newNode.incoming_weight = 0; // Reset weight after compression
            newNode.saving_so_far = new_saving;
            newNode.isPruned = 0;

            // Copy relevant compression history
            int to_copy = oldNode.compress_sequence_count - oldNode.incoming_weight + k;
            copyNodeCompressSeq(&oldNode, &newNode, to_copy);

            // Add new compressed sequence
            if (newNode.compress_sequence_count < COMPRESS_SEQUENCE_LENGTH) {
                newNode.compress_sequence[newNode.compress_sequence_count++] = seq_len;
            } else {
                fprintf(stderr, "\n\n\n Error: newNode.compress_sequence_count(%d) >= COMPRESS_SEQUENCE_LENGTH (%d)\n", 
                    newNode.compress_sequence_count, COMPRESS_SEQUENCE_LENGTH);
            }

            // Store in pool
            memcpy(&new_pool[new_nodes_count], &newNode, sizeof(TreeNode));
            new_nodes_count++;
        }
    }

    return new_nodes_count;
}
 

static inline void createRoot(uint8_t* block, int savings, int block_index) {
    // Clear both pools completely
    memset(node_pool_even, 0, sizeof(node_pool_even));
    memset(node_pool_odd, 0, sizeof(node_pool_odd));
    
    // Initialize root node properly
    TreeNode root = {0};  // Zero-initialize all fields
    root.compress_sequence[0] = 1;
    root.compress_sequence_count = 1;
    root.saving_so_far = savings;
    root.incoming_weight = 1;
    
    // Copy to pool
    memcpy(&node_pool_even[0], &root, sizeof(TreeNode));
    even_pool_count = 1;
    odd_pool_count = 0;
    
    printf("\n\n\n*************** new root created ********************");
    printNode(&node_pool_even[0], block, block_index);	
    printf("\n*******************************************************************");
}


static inline void resetToBestNode(TreeNode* source_pool, int source_count, uint8_t* block, uint32_t block_index, BinarySequence** topSeq) {
    // Find node with maximum savings
    uint32_t max_saving = 0;
    int best_index = 0;
    
    for (int i = 0; i < source_count; i++) {
        if (source_pool[i].saving_so_far > max_saving) {
            max_saving = source_pool[i].saving_so_far;
            best_index = i;
        }
    }
    printf("\n calling writeCompressedOutput");
    printf("\n====================== resting to :");
    printNode(&source_pool[best_index], block, 0);
    
    writeCompressedOutput("compress.bin", topSeq, MAX_NUMBER_OF_SEQUENCES, 
                         &source_pool[best_index], block);
                          
	createRoot(block, max_saving, block_index);  
	//todo this will be gone later on. Just for testing keep it here time being.
	exit(1);
}


void processBlockSecondPass(uint8_t* block, long blockSize, BinarySequence** topSeq) {
    if (SEQ_LENGTH_LIMIT <= 1 || blockSize <= 0 || block == NULL || !topSeq) {
        return;
    }
    createRoot(block, 0, 0);  // Start with fresh root
    uint8_t isEven = 0;  // We create root at even, and then switch to odd.
    

    uint32_t blockIndex;
    for (blockIndex = 1; blockIndex < blockSize; blockIndex++) {
        // Check if we need to prune
        if (blockIndex% COMPRESS_SEQUENCE_LENGTH == 0) {
        	if (isEven) { //valid data is in odd pool as even pool to be process next.
                resetToBestNode(node_pool_odd, odd_pool_count, block, blockIndex, topSeq);
            } else { //valid data is in even pool
                resetToBestNode(node_pool_even, even_pool_count, block, blockIndex, topSeq);
            }
            isEven = 0; //root is always at even so switch to odd pool next.  
            break;
        }

#ifdef DEBUG_PRINT
        printf("\n------------------------------------------------>Processing byte %u (pool %s)", blockIndex, isEven ? "even" : "odd");
#endif
		//printf("\t %u", i);
        // Create new nodes in the appropriate pool
        if (isEven) {
            even_pool_count = createNodes(node_pool_odd, node_pool_even, 
                                        odd_pool_count, block, blockIndex+1);
        } else {
            odd_pool_count = createNodes(node_pool_even, node_pool_odd,
                                       even_pool_count, block, blockIndex+1);
        }
        isEven = !isEven;  // Alternate pools
    }
    printf("\n------------------------------------------------>Ending \n");
	if (isEven) { //valid data is in odd pool as even pool to be process next.
        resetToBestNode(node_pool_odd, odd_pool_count, block, blockIndex, topSeq);
    } else { //valid data is in even pool
        resetToBestNode(node_pool_even, even_pool_count, block, blockIndex, topSeq);
    }
}

void processSecondPass(const char* filename, BinarySequence** topSeq) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    uint8_t* block = (uint8_t*)calloc(1, BLOCK_SIZE);
    if (!block) {
        perror("Failed to allocate memory for block");
        fclose(file);
        return;
    }

    while (1) {
        long bytesRead = fread(block, 1, BLOCK_SIZE, file);
        if (bytesRead == 0) break;

        processBlockSecondPass(block, bytesRead, topSeq);
    }
    printf("\n\n\n Potential Savings = %u\n", best_saving_overall);

    free(block);
    fclose(file);
}
