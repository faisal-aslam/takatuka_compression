// src/second_pass/second_pass.c
#include "second_pass.h"
#include <string.h>
#include <stdlib.h>
#include "../write_in_file/write_in_file.h"
#include "../weighted_freq.h"

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

void print_stacktrace(void);  
static int updateNodeMap(TreeNode *node, const uint8_t* sequence, uint16_t seq_len, uint32_t location);

static int processNodePath(TreeNode *oldNode, TreeNode *new_pool, int new_nodes_count,
                         const uint8_t* block, uint32_t block_size, uint32_t block_index,
                         int best_index[], int32_t best_saving[],
                         const uint8_t* sequence, uint16_t seq_len, uint8_t new_weight,
                         int to_copy);

static int createNodes(TreeNode *old_pool, TreeNode *new_pool, int old_node_count,
                     const uint8_t* block, uint32_t block_size, uint32_t block_index);

static int validate_block_access(const uint8_t* block, uint32_t block_size, uint32_t offset, uint16_t length) {
    if (!block || offset >= block_size || length == 0 || (offset + length) > block_size || length > block_size) {
        fprintf(stderr, "Invalid block access: offset=%u, length=%u, block_size=%u\n",
               offset, length, block_size);
        print_stacktrace();
        return 0;
    }
    return 1;
}
                         
static inline int32_t totalSavings(TreeNode *node) {
	if (!node) return 0;
	return (node->saving_so_far-node->headerOverhead);
}
static void printNode(TreeNode *node, const uint8_t* block, uint32_t block_index) {
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
    binseq_map_print(node->map);
}

void cleanup_node_pools() {
    for (int i = 0; i < MAX_TREE_NODES; i++) {
        if (node_pool_even[i].map) {
            binseq_map_free(node_pool_even[i].map);
            node_pool_even[i].map = NULL;

        }
        if (node_pool_odd[i].map) {
            binseq_map_free(node_pool_odd[i].map);
            node_pool_odd[i].map = NULL;
        }
    }
}

/**
 * Calculates potential savings of compressing a sequence, WITHOUT modifying any data.
 * Returns:
 *   - savings in bits if compression is beneficial (positive)
 *   - negative savings if compression is not beneficial or invalid
 */
static int32_t calculateSavings(const uint8_t* newBinSeq, uint16_t seq_length, BinSeqMap *map) {
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
static int updateMapValue(BinSeqValue* binSeqVal, int block_index, uint16_t seq_length) {
    if (!binSeqVal) {
    	fprintf(stderr, "\n Incorrect input to function updateMapValue");
    	return 0;
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
static inline BinarySequence* isValidSequence(uint16_t sequence_length, const uint8_t* block, uint32_t block_index) {
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
                     const uint8_t* block, uint32_t block_size, uint32_t block_index) {
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
            //todo continue;
        }

        #ifdef DEBUG        
        printf("\n******************** Processing OLD node ************");
        printNode(oldNode, block, block_index);
        printf("\n******************************************************");
        #endif
        

        // Process uncompressed path
        /*processNodePath(TreeNode *oldNode, TreeNode *new_pool, int new_nodes_count,
                         const uint8_t* block, uint32_t block_size, uint32_t block_index,
                         int best_index[], int32_t best_saving[],
                         const uint8_t* sequence, uint16_t seq_len, uint8_t new_weight,
                         int to_copy)*/
        new_nodes_count = processNodePath(oldNode, new_pool, new_nodes_count,
                                        block, block_size, block_index, best_index, best_saving,
                                        &block[block_index], 1, oldNode->incoming_weight + 1,
                                        oldNode->compress_sequence_count);
        
        
        // Process compressed paths
		for (int k = 0; k < oldNode->incoming_weight; k++) {
			uint16_t seq_len = oldNode->incoming_weight + 1 - k;
			
			// Validate sequence length and block boundaries
			if (seq_len < SEQ_LENGTH_START || seq_len > SEQ_LENGTH_LIMIT || 
				seq_len > (block_index + 1) || (block_index + 1 - seq_len) >= block_size) {
				fprintf(stderr, "Invalid seq len=%d at block_index=%d, block_size=%d\n", 
				       seq_len, block_index, block_size);
				continue;
			}
			
			const uint8_t* seq_start = &block[block_index + 1 - seq_len];
			new_nodes_count = processNodePath(oldNode, new_pool, new_nodes_count,
				                            block, block_size, block_index, best_index, best_saving,
				                            seq_start, seq_len, 0,
				                            oldNode->compress_sequence_count - oldNode->incoming_weight + k);
		}
        
    }

    return new_nodes_count;
}

static void print_bin_seq(const uint8_t* sequence, uint16_t seq_len) {
	if (!sequence) return;
	printf("\n printing sequence of length=%d = {", seq_len);
	for (int i =0; i < seq_len; i++) {
		printf("0x%x, \t", sequence[i]);
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
static int updateNodeMap(TreeNode *node, const uint8_t* sequence, uint16_t seq_len, uint32_t location) {
    // Validate sequence access
    if (location >= BLOCK_SIZE || (location + seq_len) > BLOCK_SIZE) {
        fprintf(stderr, "Invalid location in updateNodeMap: loc=%u, len=%u\n", location, seq_len);
        return 0;
    }
   
    BinSeqKey key = create_binseq_key(sequence, seq_len);
    if (!key.binary_sequence) {
        return 0;
    }
 
    BinSeqValue* value = binseq_map_get(node->map, key);
    int result = 0;
    
    if (value) {
        // If value exists (already in map)
        #ifdef DEBUG
        printf("\n already in the map \n");
        #endif
        result = updateMapValue(value, location, seq_len);
        free_key(key);  // Free the lookup key
    } else {
        #ifdef DEBUG    
        printf("\n Not in the map \n");
        #endif
        // New sequence - the map will take ownership of the key
        uint32_t locs[1] = {location};
        BinSeqValue newValue = create_binseq_value(1, locs, 1);
    
       
        if (binseq_map_put(node->map, key, newValue)) {
   	        #ifdef DEBUG
        	printf("\n map is updated \n");
        	binseq_map_print(node->map);
        	#endif
            result = 1;
            if (seq_len > 1) //todo later will compress for seq_len = 1.
                node->headerOverhead += getHeaderOverhead((seq_len == 1) ? 0 : 3, seq_len);
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
                         const uint8_t* block, uint32_t block_size, uint32_t block_index,
                         int best_index[], int32_t best_saving[],
                         const uint8_t* sequence, uint16_t seq_len, uint8_t new_weight,
                         int to_copy) { 
  // Block size validation
    if (block_index >= block_size) {
        fprintf(stderr, "Invalid block_index %u >= block_size %u\n", block_index, block_size);
        return new_nodes_count;
    }
    
    // Sequence length validation
    if (seq_len == 0 || seq_len > COMPRESS_SEQUENCE_LENGTH) {
        fprintf(stderr, "Invalid seq_len %u\n", seq_len);
        return new_nodes_count;
    }
    
    // Validate we're not reading past block boundaries
    uint32_t seq_start_offset = (new_weight == 0) ? block_index + 1 - seq_len : block_index;
    if (seq_start_offset >= block_size || (seq_start_offset + seq_len) > block_size) {
        fprintf(stderr, "Invalid sequence access: offset=%u, len=%u, block_size=%u\n",
               seq_start_offset, seq_len, block_size);
        return new_nodes_count;
    }
    
    BinSeqKey key = create_binseq_key(sequence, seq_len);
    if (!key.binary_sequence) {
        fprintf(stderr, "\n unable to make key \n");
        return new_nodes_count;
    }
	
    // Calculate new savings
    int32_t new_saving = calculateSavings(sequence, seq_len, oldNode->map) + oldNode->saving_so_far;
    uint8_t isPruned = 0;
    // Check if already a better path exist with same weight (pruning).
    if (!(best_index[new_weight] == -1 || new_saving - oldNode->headerOverhead <= best_saving[new_weight])) {
        #ifdef DEBUG
        printf("\n\n Pruned \n");
        #endif
        isPruned = 1;
        //todo free_key(key);
        //todo return new_nodes_count;
    }

    // Initialize new node - ensure complete zero initialization
    TreeNode newNode = {0};  // This ensures all pointers start as NULL
    
    // Configure node properties
    newNode.incoming_weight = (new_weight >= SEQ_LENGTH_LIMIT) ? SEQ_LENGTH_LIMIT - 1 : new_weight;
    newNode.headerOverhead = oldNode->headerOverhead;
    newNode.saving_so_far = new_saving;
    newNode.isPruned = isPruned;

    // Copy sequences
    if (to_copy > 0 && to_copy < COMPRESS_SEQUENCE_LENGTH) {
        copySequences(oldNode, &newNode, to_copy);
    }

    // Deep copy the map - handles ownership transfer internally
    copyMap(oldNode, &newNode);

    // Add new sequence to the node
    if (newNode.compress_sequence_count >= COMPRESS_SEQUENCE_LENGTH) {
        fprintf(stderr, "\nError: compress_sequence_count overflow\n");
        free_key(key);
        return new_nodes_count;
    }
    newNode.compress_sequence[newNode.compress_sequence_count++] = seq_len;

    // Update node map
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

    /* ===== CRITICAL MEMORY MANAGEMENT SECTION ===== */
    // 1. Free any existing map in the target node first
    if (new_pool[new_nodes_count].map) {
        binseq_map_free(new_pool[new_nodes_count].map);
        new_pool[new_nodes_count].map = NULL;
    }

    // 2. Copy all non-pointer members
    memcpy(&new_pool[new_nodes_count], &newNode, sizeof(TreeNode));
    
    // 3. Transfer map ownership carefully
    new_pool[new_nodes_count].map = newNode.map;
    newNode.map = NULL;  // Prevents double-free
    
    // 4. Free the temporary key
    free_key(key);
    /* ===== END CRITICAL SECTION ===== */

    #ifdef DEBUG
    printNode(&new_pool[new_nodes_count], block, block_index);
    #endif

    return new_nodes_count + 1;
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

static inline void createRoot(const uint8_t* block, uint32_t block_size) {
    if (!block || !validate_block_access(block, block_size, 0, 1)) {
        fprintf(stderr, "Error: block is NULL or invalid in createRoot\n");
        return;
    }

    // Clear both node pools
    cleanup_node_pools();
    memset(node_pool_even, 0, sizeof(node_pool_even));
    memset(node_pool_odd, 0, sizeof(node_pool_odd));

    // Initialize root node
    TreeNode root = {0};
    root.headerOverhead = 4;
    root.compress_sequence[0] = 1;
    root.compress_sequence_count = 1;
    root.incoming_weight = 1;

    root.map = binseq_map_create(1);
    if (!root.map) {
        fprintf(stderr, "FATAL: Failed to create root map\n");
        exit(EXIT_FAILURE);
    }

    // Prepare key and value
    uint32_t locs[1] = {0};
    BinSeqKey key = create_binseq_key(&block[0], 1);
    if (!key.binary_sequence) {
        binseq_map_free(root.map);
        return;
    }

    BinSeqValue value = create_binseq_value(1, locs, 1);
    if (!binseq_map_put(root.map, key, value)) {
        free_key(key);
        free_binseq_value(value);
        binseq_map_free(root.map);
        return;
    }

    // Calculate savings AFTER map has been populated
    root.saving_so_far = calculateSavings(&block[0], 1, root.map);

    // Transfer ownership to node_pool_even[0]
    if (node_pool_even[0].map) {
        binseq_map_free(node_pool_even[0].map);
        node_pool_even[0].map = NULL;
    }

    node_pool_even[0] = root;
    root.map = NULL;  // Prevent double-free

    even_pool_count = 1;
    odd_pool_count = 0;
	#ifdef DEBUG
    printf("\n\n*************** new root created ********************");
    printNode(&node_pool_even[0], block, 0);    
    printf("\n*******************************************************************");
    #endif
}


static inline void resetToBestNode(TreeNode* source_pool, int source_count, const uint8_t* block, uint32_t block_index) {
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
    #ifdef DEBUG
    printf("\n calling writeCompressedOutput");
    printf("\n====================== resting to :");
    printNode(&source_pool[best_index], block, 0);
    #endif
    
    //writeCompressedOutput("compress.bin", topSeq, MAX_NUMBER_OF_SEQUENCES, 
      //                   &source_pool[best_index], block);
                          
	//createRoot(block, max_saving, block_index);  
	//todo this will be gone later on. Just for testing keep it here time being.
	cleanup_node_pools();
	exit(1);

}


void processBlockSecondPass(const uint8_t* block, uint32_t block_size) {
    if (SEQ_LENGTH_LIMIT <= 1 || block_size <= 0 || block == NULL) {
        fprintf(stderr, "Error: Invalid parameters in processBlockSecondPass\n");
        cleanup_node_pools();  // Add this
        return;
    }
    createRoot(block, block_size);  // Start with fresh root
    uint8_t isEven = 0;  // We create root at even, and then switch to odd.


    uint32_t blockIndex;
    uint8_t restedOnce = 0;
    for (blockIndex = 1; blockIndex < block_size; blockIndex++) {
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
                                        odd_pool_count, block, block_size, blockIndex);
        } else {
            odd_pool_count = createNodes(node_pool_even, node_pool_odd,
                                       even_pool_count, block, block_size, blockIndex);
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

    uint8_t* block = (uint8_t*)malloc(BLOCK_SIZE); // Use malloc instead of calloc
    if (!block) {
        perror("Failed to allocate memory for block");
        fclose(file);
        cleanup_node_pools();
        return;
    }

    while (1) {
        long bytesRead = fread(block, 1, BLOCK_SIZE, file);
        if (bytesRead <= 0) break;  // Check for <= 0
      
        processBlockSecondPass(block, bytesRead);
        
    }
    
    free(block);
    fclose(file);
    cleanup_node_pools();  // Ensure final cleanup
}
