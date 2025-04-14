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

static int updateNodeMap(TreeNode *node, uint8_t* sequence, uint16_t seq_len, uint32_t location);
static int processNodePath(TreeNode *oldNode, TreeNode *new_pool, int new_nodes_count,
                         uint8_t* block, uint32_t block_index,
                         int best_index[], int32_t best_saving[],
                         uint8_t* sequence, uint16_t seq_len, uint8_t new_weight,
                         int to_copy);
                         
static inline int32_t totalSavings(TreeNode *node) {
	if (!node) return 0;
	return (node->saving_so_far-node->headerOverhead);
}
static void printNode(TreeNode *node, uint8_t* block, uint32_t block_index) {
    if (!node) {
        printf("[NULL NODE]\n");
        return;
    }
    
    // Use direct values but add bounds checking
    printf("\n Node :");
    printf("saving_so_far=%d, headerOverhead=%d, incoming_weight=%d, isPruned=%d, compress_seq_count=%d", 
           node->saving_so_far, 
           node->headerOverhead,
           node->incoming_weight, 
           node->isPruned, 
           (node->compress_sequence_count < COMPRESS_SEQUENCE_LENGTH) ? node->compress_sequence_count : 0);
    printf(", total Savings =%d", totalSavings(node));
    // Bounds checking
    if (node->compress_sequence_count > COMPRESS_SEQUENCE_LENGTH) {
        printf("\n\t[ERROR: Invalid compress_sequence_count]");
        return;
    }
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
    //binseq_map_print(node->map);
}

void cleanup_node_pools() {
    for (int i = 0; i < MAX_TREE_NODES; i++) {
        if (node_pool_even[i].map) {
            binseq_map_free(node_pool_even[i].map);
        }
        if (node_pool_odd[i].map) {
            binseq_map_free(node_pool_odd[i].map);
        }
    }
}

/**
 * Calculates potential savings of compressing a sequence, WITHOUT modifying any data.
 * Returns:
 *   - savings in bits if compression is beneficial (positive)
 *   - negative savings if compression is not beneficial or invalid
 */
static int32_t calculateSavings(uint8_t* newBinSeq, uint16_t seq_length, BinSeqMap *map) {
    if (!newBinSeq || seq_length <= 0 || seq_length > COMPRESS_SEQUENCE_LENGTH || !map) {
        fprintf(stderr, "Error: Invalid parameters in calculateSavings\n");
        return 0;
    }
    
    // Create the key
    BinSeqKey key = create_binseq_key(newBinSeq, seq_length);  
     
    BinSeqValue* binSeqVal = binseq_map_get(map, key);
    long savings = 0;
    
    if (!binSeqVal || seq_length == 1) {//todo: later we will support compress of 1 byte. Not at the moment.
	    //in this case savings will be negative as each uncompress byte will start with an extra 0. (9-bits for a byte).
        savings = -(long)(seq_length); 
    } else {
        int group = (seq_length == 1) ? 0 : 3;
        long originalSizeBits = (long)(seq_length * 8 /*+ seq_length*/);
        long compressedSizeBits = (long)(groupCodeSize(group) + groupOverHead(group));
        savings = originalSizeBits - compressedSizeBits;
        if (savings < 0) {
            fprintf(stderr, "\nSavings is less than zero after compression \n");
            free(key.binary_sequence);
            return 0;
        }
    }
    
    free(key.binary_sequence); // Clean up
    return savings; //sending positive or negative savings.
}


/**
 * Updates frequency and location for a known existing sequence in the map.
 * Also updates header overhead if this is the first time it is compressed.
 */
static int updateMapValue(BinSeqValue* binSeqVal, int block_index, uint16_t seq_length, int* headerOverhead) {
    if (!binSeqVal || !headerOverhead) {
    	fprintf(stderr, "\n Incorrect input to function updateMapValue");
    	return 0;
    }
    int group = (seq_length == 1) ? 0 : 3;

    if (binSeqVal->frequency == 1) {
        *headerOverhead += getHeaderOverhead(group, seq_length);
    } 

    if (!binseq_value_append_location(binSeqVal, block_index)) {
        fprintf(stderr, "Failed to append location to sequence\n");
    }
    binSeqVal->frequency++;
    return 1;
} 


static inline void copySequences(TreeNode *sourceNode, TreeNode *destNode, uint16_t bytes_to_copy) {
    // Validate inputs
    if (!sourceNode || !destNode || 
        bytes_to_copy == 0 || 
        bytes_to_copy > COMPRESS_SEQUENCE_LENGTH || 
        bytes_to_copy > sourceNode->compress_sequence_count) { 
        fprintf(stderr, "\nInvalid copy parameters: src_count=%u, to_copy=%u\n",
               sourceNode ? sourceNode->compress_sequence_count : 0,
               bytes_to_copy);
        return;
    }
    
    // Copy exactly the requested number of elements
    memcpy(destNode->compress_sequence, 
           sourceNode->compress_sequence, 
           bytes_to_copy * sizeof(sourceNode->compress_sequence[0])); 
    
    destNode->compress_sequence_count = bytes_to_copy;
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
    int best_index[SEQ_LENGTH_LIMIT+1];
    int32_t best_saving[SEQ_LENGTH_LIMIT+1];
    
    for (int i = 0; i < SEQ_LENGTH_LIMIT+1; i++) {
        best_saving[i] = INT_MIN;
        best_index[i] = -1;
    }
 
    #ifdef DEBUG
    printf("\nold_node_count= %d", old_node_count);
    #endif

    int new_nodes_count = 0;
    for (int j = 0; j < old_node_count; j++) {
        TreeNode *oldNode = &old_pool[j];
        
        // Skip pruned nodes
        if (oldNode->isPruned) {
            continue;
        }

        #ifdef DEBUG        
        printf("\n******************** Processing OLD node ************");
        printNode(oldNode, block, block_index);
        printf("\n******************************************************");
        #endif
        
        printf("\n process uncompressed path block=0x%x, index=%d\n", block[block_index], block_index);
        // Process uncompressed path
        new_nodes_count = processNodePath(oldNode, new_pool, new_nodes_count,
                                        block, block_index, best_index, best_saving,
                                        &block[block_index], 1, oldNode->incoming_weight + 1,
                                        oldNode->compress_sequence_count);
        
        
        // Process compressed paths
        for (int k = 0; k < oldNode->incoming_weight; k++) {
            printf("\n process uncompressed path \n");
            uint16_t seq_len = oldNode->incoming_weight + 1 - k;
            
            // Validate sequence length
            if (seq_len < SEQ_LENGTH_START || seq_len > SEQ_LENGTH_LIMIT) {
                fprintf(stderr, "Invalid seq length seq_len=%d\n", seq_len);
                continue; 
            }
            
            // Check if we have enough bytes left in block
            if (block_index + 1 < seq_len) {
                fprintf(stderr, "Invalid seq_len=%d at block_index=%d\n", seq_len, block_index);
                exit(EXIT_FAILURE); 
            }
            
            uint8_t* seq_start = &block[block_index + 1 - seq_len];
            new_nodes_count = processNodePath(oldNode, new_pool, new_nodes_count,
                                            block, block_index, best_index, best_saving,
                                            seq_start, seq_len, 0,
                                            oldNode->compress_sequence_count - oldNode->incoming_weight + k);
        }
    }

    return new_nodes_count;
}
static void print_bin_seq(uint16_t* sequence, uint16_t seq_len) {
	if (!sequence) return;
	printf("\n printing sequence of length=%d = {", seq_len);
	for (int i =0; i < seq_len; i++) {
		printf("0x%x, ", sequence[i]);
	}
	printf(" }\n");
}
/**
 * Updates the node's map with a new sequence
 * 
 * @param node        Node to update
 * @param sequence    Pointer to sequence data
 * @param seq_len     Length of sequence
 * @param location    Location in block where sequence starts
 * @return            1 on success, 0 on failure
 */
static int updateNodeMap(TreeNode *node, uint8_t* sequence, uint16_t seq_len, uint32_t location) {
    BinSeqKey key = create_binseq_key(sequence, seq_len);
    if (!key.binary_sequence) {
        return 0;
    }
    //print_bin_seq(sequence, seq_len);
    
	//binseq_map_print(node->map);
	
    BinSeqValue* value = binseq_map_get(node->map, key);
    int result = 0;
    
    if (value) {
        // If value exists (already in map)
        printf("\n already in the map \n");
        result = updateMapValue(value, location, seq_len, &node->headerOverhead);
        free_key(key);  // Free the lookup key
    } else {
        printf("\n Not in the map \n");
        // New sequence - the map will take ownership of the key
        uint32_t locs[1] = {location};
        BinSeqValue newValue = create_binseq_value(1, locs, 1);
    
       
        if (binseq_map_put(node->map, key, newValue)) {
            result = 1;
            // binseq_map_put now owns the key, don't free it here
        } else {
            free_key(key);
            free_binseq_value(newValue);
        }
    }
    
    return result;
}

/**
 * Processes a node path (either compressed or uncompressed)
 */
static int processNodePath(TreeNode *oldNode, TreeNode *new_pool, int new_nodes_count,
                         uint8_t* block, uint32_t block_index,
                         int best_index[], int32_t best_saving[],
                         uint8_t* sequence, uint16_t seq_len, uint8_t new_weight,
                         int to_copy) {
    BinSeqKey key = create_binseq_key(sequence, seq_len);
    //print_bin_seq(sequence, seq_len);
    if (!key.binary_sequence) {
    	fprintf(stderr, "\n unable to make key \n");
        return new_nodes_count;
    }

	
    // Calculate new savings
    int32_t new_saving = calculateSavings(sequence, seq_len, oldNode->map) + oldNode->saving_so_far;
    
    // Check if already a better path exist with same weight
    if (!(best_index[new_weight] == -1 || new_saving - oldNode->headerOverhead <= best_saving[new_weight])) {
        free_key(key);
        return new_nodes_count;
    }

    // Initialize new node
    TreeNode newNode = {0};
    
    // Configure common node properties
    newNode.incoming_weight = (new_weight >= SEQ_LENGTH_LIMIT) ? SEQ_LENGTH_LIMIT - 1 : new_weight;
    newNode.headerOverhead = oldNode->headerOverhead;
    newNode.saving_so_far = new_saving;
    newNode.isPruned = 0;
  
 
    // Copy sequences from old node if needed
    if (to_copy > 0 && to_copy <= COMPRESS_SEQUENCE_LENGTH) {
        copySequences(oldNode, &newNode, to_copy);
    }
    
    // Copy map from old node - this now properly handles key ownership
    copyMap(oldNode, &newNode);
    
    // Add new sequence to the node
    if (newNode.compress_sequence_count >= COMPRESS_SEQUENCE_LENGTH) {
        fprintf(stderr, "\n\n\n Error: newNode.compress_sequence_count(%d) >= COMPRESS_SEQUENCE_LENGTH (%d)\n", 
            newNode.compress_sequence_count, COMPRESS_SEQUENCE_LENGTH);
        free_key(key);
        return new_nodes_count;
    }
    printf("\n before adding the sequence node newNode.compress_sequence are: ");
    print_bin_seq(newNode.compress_sequence, newNode.compress_sequence_count);
    printf("\n Going to add a new seq_len=%d, at index=%d ", seq_len, newNode.compress_sequence_count);
    newNode.compress_sequence[newNode.compress_sequence_count++] = seq_len;
    
    printf("\n before adding the sequence node newNode.compress_sequence are: ");
    print_bin_seq(newNode.compress_sequence, newNode.compress_sequence_count);
    
    newNode.headerOverhead = oldNode->headerOverhead;
    // Update node map with the new sequence - this handles key ownership
    if (!updateNodeMap(&newNode, sequence, seq_len, 
                      (new_weight == 0) ? block_index + 1 - seq_len : block_index)) {
        free_key(key);
        return new_nodes_count;
    }
    
    // Prune previous best if exists
    int weight_index = (new_weight == 0) ? 0 : new_weight;
    if (best_index[weight_index] != -1) {
        new_pool[best_index[weight_index]].isPruned = 1;
    }
    
    // Update tracking
    best_saving[weight_index] = new_saving - oldNode->headerOverhead;
    best_index[weight_index] = new_nodes_count;
    
    // Store in pool
    new_pool[new_nodes_count] = newNode;  // shallow copy
    new_pool[new_nodes_count].map = newNode.map;  // this is already deep-copied by copyMap


    
    #ifdef DEBUG
    printNode(&new_pool[new_nodes_count], block, block_index);
    #endif

    new_nodes_count++;
    free_key(key);  // Free the key we created at the start
    return new_nodes_count;
}



/**
 * Creates a binary sequence from the block at given position
 */
static uint8_t* createBinSeq(uint16_t seq_len, uint8_t* block, uint32_t block_index) {
    if (!block || seq_len == 0 || block_index + 1 < seq_len) {
        return NULL;
    }
    
    uint8_t* seq = malloc(seq_len);
    if (!seq) {
        return NULL;
    }
    
    memcpy(seq, &block[block_index + 1 - seq_len], seq_len);
    return seq;
}

static inline void createRoot(uint8_t* block, int32_t savings, int block_index) {

    if (!block) {
        fprintf(stderr, "Error: block is NULL in createRoot\n");
        return;
    }

    // Clear both pools completely
    memset(node_pool_even, 0, sizeof(node_pool_even));
    memset(node_pool_odd, 0, sizeof(node_pool_odd));
    
    // Initialize root node properly
    TreeNode root = {0};  // Zero-initialize all fields
    root.headerOverhead = 4; //the 4-bytes about the number of sequences in header.
    root.compress_sequence[0] = 1;
    root.compress_sequence_count = 1;
    root.map = binseq_map_create(16); //create map of root.
    if (!root.map) {
	    fprintf(stderr, "FATAL: Failed to create root map\n");
	    exit(EXIT_FAILURE);
	}
    
    root.saving_so_far = calculateSavings(&block[0], 1, root.map);
    root.incoming_weight = 1;
    
	uint32_t locs[1] = {0};
	BinSeqKey key = create_binseq_key(&block[0], 1);
	
	BinSeqValue value = create_binseq_value(1, locs, 1);
	if (!binseq_map_put(root.map, key, value)) {
    	return;
	}

    // Copy to pool
    memcpy(&node_pool_even[0], &root, sizeof(TreeNode));
    even_pool_count = 1;
    odd_pool_count = 0;

    printf("\n\n*************** new root created ********************");
    printNode(&node_pool_even[0], block, block_index);    
    printf("\n*******************************************************************");
}


static inline void resetToBestNode(TreeNode* source_pool, int source_count, uint8_t* block, uint32_t block_index) {
    // Find node with maximum savings
    int32_t max_saving = INT_MIN;
    int best_index = 0;
    
    for (int i = 0; i < source_count; i++) {
    	//printf("\ncomparing total Saving =%d with max_savings=%d", totalSavings(&source_pool[i]), max_saving);
        if (totalSavings(&source_pool[i]) > max_saving) {
            max_saving = totalSavings(&source_pool[i]);
            best_index = i;
        }
    }
    printf("\n calling writeCompressedOutput");
    printf("\n====================== resting to :");
    printNode(&source_pool[best_index], block, 0);
    
    //writeCompressedOutput("compress.bin", topSeq, MAX_NUMBER_OF_SEQUENCES, 
      //                   &source_pool[best_index], block);
                          
	//createRoot(block, max_saving, block_index);  
	//todo this will be gone later on. Just for testing keep it here time being.
	exit(1);
}


void processBlockSecondPass(uint8_t* block, long blockSize) {
    if (SEQ_LENGTH_LIMIT <= 1 || blockSize <= 0 || block == NULL) {
        fprintf(stderr, "Error: Invalid parameters in processBlockSecondPass\n");
        cleanup_node_pools();  // Add this
        return;
    }
    createRoot(block, 0, 0);  // Start with fresh root
    uint8_t isEven = 0;  // We create root at even, and then switch to odd.


    uint32_t blockIndex;
    uint8_t restedOnce = 0;
    for (blockIndex = 1; blockIndex < blockSize; blockIndex++) {
        // Check if we need to reset
        if (blockIndex% COMPRESS_SEQUENCE_LENGTH == 0) {
           restedOnce = 1; //todo for testing. Should be remove later.
        	printf("\n\n\n Reached limit blockIndex=%u, COMPRESS_SEQUENCE_LENGTH=%d \n", blockIndex, COMPRESS_SEQUENCE_LENGTH);
        	if (isEven) { //valid data is in odd pool as even pool to be process next.
                resetToBestNode(node_pool_odd, odd_pool_count, block, blockIndex);
            } else { //valid data is in even pool
                resetToBestNode(node_pool_even, even_pool_count, block, blockIndex);
            }
            isEven = 0; //root is always at even so switch to odd pool next.  
        }

#ifdef DEBUG
        printf("\n------------------------------------------------>Processing byte %u (pool %s)", blockIndex, isEven ? "even" : "odd");
#endif
		//printf("\t %u", i);
        // Create new nodes in the appropriate pool
        if (isEven) {
            even_pool_count = createNodes(node_pool_odd, node_pool_even, 
                                        odd_pool_count, block, blockIndex);
        } else {
            odd_pool_count = createNodes(node_pool_even, node_pool_odd,
                                       even_pool_count, block, blockIndex);
        }
        /*if(1) {
        	printf(" \n\n Ending \n\n ");
        	return;
        }*/
        isEven = !isEven;  // Alternate pools
    }
    printf("\n------------------------------------------------>Ending \n");
    if (!restedOnce) {
		if (isEven) { //valid data is in odd pool as even pool to be process next.
    	    resetToBestNode(node_pool_odd, odd_pool_count, block, blockIndex);
    	} else { //valid data is in even pool
    	    resetToBestNode(node_pool_even, even_pool_count, block, blockIndex);
    	}
    }
    //cleanup.
    cleanup_node_pools();
}

void processSecondPass(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        cleanup_node_pools(); 
        return;
    }

    uint8_t* block = (uint8_t*)calloc(1, BLOCK_SIZE);
    if (!block) {
        perror("Failed to allocate memory for block");
        fclose(file);
        cleanup_node_pools();  // Add this
        return;
    }

    while (1) {
        long bytesRead = fread(block, 1, BLOCK_SIZE, file);
        if (bytesRead == 0) break;

        processBlockSecondPass(block, bytesRead);
    }
    //printf("\n\n\n Potential Savings = %u\n", best_saving_overall);

    free(block);
    fclose(file);
}
