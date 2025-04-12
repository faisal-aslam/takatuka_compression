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

static void printNode(TreeNode *node, uint8_t* block, uint32_t block_index) {
    if (!node) {
        printf("[NULL NODE]\n");
        return;
    }
    
    // Use direct values but add bounds checking
    printf("\n Node :");
    printf("saving_so_far=%ld, incoming_weight=%d, isPruned=%d, compress_seq_count=%d", 
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
    binseq_map_print(node->map);
}

/**
 * Calculates potential savings of compressing a sequence, WITHOUT modifying any data.
 * Returns:
 *   - savings in bits if compression is beneficial (positive)
 *   - negative savings if compression is not beneficial or invalid
 */
static long calculateSavings(uint8_t* newBinSeq, uint16_t seq_length, BinSeqMap *map) {
    if (!newBinSeq || seq_length <= 0 || seq_length > COMPRESS_SEQUENCE_LENGTH || !map) {
        fprintf(stderr, "Error: Invalid parameters in calculateSavings\n");
        return 0;
    }
    
    // Create the key
    BinSeqKey key = create_binseq_key(newBinSeq, seq_length);  
     
    BinSeqValue* binSeqVal = binseq_map_get(map, key);
    long savings = 0;
    
    if (!binSeqVal) {
	    //in this case savings will be negative as each uncompress byte will start with an extra 0. (9-bits for a byte).
        savings = -(long)(seq_length); 
    } else {
        int group = (seq_length == 1) ? 1 : 4;
        long originalSizeBits = (long)(seq_length * 8 + seq_length);
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
 * Inserts a new sequence into the node's map. Returns a pointer to the newly created BinSeqValue.
 * Updates seqLocation and sets frequency = 1.
 * On failure, returns 0.
 */
static int insertMapValue(uint8_t* newBinSeq, uint16_t seq_length, TreeNode *node, int block_index) {
    if (!newBinSeq || seq_length <= 0 || seq_length > COMPRESS_SEQUENCE_LENGTH || !node || !node->map) return 0;

    BinSeqKey key = create_binseq_key(newBinSeq, seq_length);
    uint32_t loc = block_index;
    BinSeqValue newVal = create_binseq_value(1, &loc, 1);
    
    if (!binseq_map_put(node->map, key, newVal)) {
        fprintf(stderr, "\nUnable to create map \n");
        free_key(key);
        return 0;
    }

    free_key(key);
    return 1;
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
    int group = (seq_length == 1) ? 1 : 4;

    if (binSeqVal->frequency == 1) {
        *headerOverhead += getHeaderOverhead(group, seq_length);
    }

    if (!binseq_value_append_location(binSeqVal, block_index)) {
        fprintf(stderr, "Failed to append location to sequence\n");
    }
    binSeqVal->frequency++;
    return 1;
} 


/**
 * Calculates savings for a given binary sequence in a compression context.
 *
 * - If the sequence is not present in the node's map:
 *     - Adds it to the map with initial location and frequency.
 *     - Computes negative savings (expansion cost).
 * - If the sequence already exists:
 *     - Adds the new location to the map.
 *     - Updates frequency and calculates compression savings.
 *     - If it is the first time compressing this sequence, updates the header overhead.
 *
 * Returns:
 *     The number of bits saved (positive if compression helps, negative if not).
 *
static inline long calculateSavings_OLD(uint8_t* newBinSeq, uint16_t seq_length, TreeNode *oldNode, int* headerOverhead) {
    // Step 1: Validate inputs
    if (!newBinSeq || seq_length <= 0 || seq_length > COMPRESS_SEQUENCE_LENGTH || !oldNode || !headerOverhead) {
        fprintf(stderr, "\n[Error] Invalid input to function calculateSavings.\n");
        return 0;
    }

    BinSeqMap *map = oldNode->map;
    if (!map) {
        fprintf(stderr, "\n[Error] Map in the TreeNode is NULL.\n");
        return 0;
    }

    // Step 2: Construct key for the binary sequence
    BinSeqKey key = { .length = seq_length };
    memcpy(key.binary_sequence, newBinSeq, seq_length);

    BinSeqValue* binSeqVal = binseq_map_get(map, key);
    long savings = 0;

    if (!binSeqVal) {
        // Sequence not found: insert it with negative savings (expansion due to overhead)

        binSeqVal = (BinSeqValue*)calloc(1, sizeof(BinSeqValue));
        if (!binSeqVal) {
            fprintf(stderr, "\n[Error] Memory allocation failed for BinSeqValue.\n");
            return 0;
        }

        binSeqVal->frequency = 1;

        if (seqLocationLength >= MAX_SEQ_LOCATIONS) {
            fprintf(stderr, "\n[Error] Exceeded maximum allowed sequence locations.\n");
            free(binSeqVal);
            return 0;
        }

        binSeqVal->seqLocation[seqLocationLength++] = oldNode->compress_sequence_count;

        if (!binseq_map_put(map, key, binSeqVal)) {
            fprintf(stderr, "\n[Error] Failed to insert sequence into map (possibly full).\n");
            free(binSeqVal);
            return 0;
        }

        // Expansion overhead: we need to add a zero before every byte in original
        // (1 extra bit per byte). So cost is (1 * number of bytes) in bits = seq_length bits
        savings = -(long)(seq_length * 8);
    } else {
        // Sequence found: compute compression savings

        int group = (seq_length == 1) ? 1 : 4;

        if (binSeqVal->frequency == 1) {
            // First compression usage of this sequence: update header overhead
            *headerOverhead = getHeaderOverhead(group, seq_length);
            // You could optionally update savings of all existing locations here.
        }

        if (seqLocationLength >= MAX_SEQ_LOCATIONS) {
            fprintf(stderr, "\n[Error] Exceeded maximum allowed sequence locations.\n");
            return 0;
        }

        binSeqVal->seqLocation[seqLocationLength++] = oldNode->compress_sequence_count;
        binSeqVal->frequency++;

        // Calculate actual savings:
        // Original size in bits = 8 bits per byte + 1 extra bit (prefix) = seq_length * 9
        // Compressed size = groupCodeSize + groupOverhead
        long originalSizeBits = (long)(seq_length * 8 + seq_length);
        long compressedSizeBits = (long)(groupCodeSize(group) + groupOverHead(group));
        savings = originalSizeBits - compressedSizeBits;

        if (savings < 0) {
            fprintf(stderr, "\n[Warning] Negative savings detected for sequence.\n");
            return 0;
        }
    }

    return savings;
}
*/

/*
long savings = calculateSavings(seq, seq_len, oldNode);

BinSeqKey key = { .length = seq_len };
memcpy(key.binary_sequence, seq, seq_len);
BinSeqValue* existing = binseq_map_get(oldNode->map, key);

if (!existing) {
    BinSeqValue* newVal = insertNewSequence(seq, seq_len, oldNode, oldNode->compress_sequence_count);
    if (!newVal) {
        fprintf(stderr, "Failed to insert new sequence.\n");
        // handle error
    }
} else {
    updateExistingSequence(existing, oldNode->compress_sequence_count, seq_len, &headerOverhead);
}
*/

static inline void copyNodeData(TreeNode *sourceNode, TreeNode *destNode, uint16_t bytes_to_copy, long newSavings) {
    // check input.
    if (!sourceNode || !destNode || bytes_to_copy == 0 || bytes_to_copy > COMPRESS_SEQUENCE_LENGTH) {
        return;
    }
    /*
    * Step 1: Copy sequences and update destNode sequence count.
    */
    memcpy(destNode->compress_sequence, sourceNode->compress_sequence, bytes_to_copy); 
    destNode->compress_sequence_count = bytes_to_copy;
    
    /*
    * Step 2: Copy binary sequences map. Increase size of new map by +1
    */
    copyMap(sourceNode, destNode);
    
    /*
    * Step 3: set new savings.
    */
    destNode->saving_so_far = newSavings;
     
    /*
    * By default prune is false.
    */
    destNode->isPruned = 0;
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
    long best_saving[(block_index+1)*(block_index+2)];
    int best_index[(block_index+1)*(block_index+2)];
    
    int best_length = sizeof(best_saving)/sizeof(best_saving[0]);
    
    for (int i = 0; i < best_length; i++) {
        best_saving[i] = LONG_MIN; //savings can be negative.
        best_index[i] = -1;
    }
 
    #ifdef DEBUG_PRINT
    printf("\nold_node_count= %d", old_node_count);
    #endif

    int new_nodes_count = 0;
    for (int j = 0; j < old_node_count; j++) {
        TreeNode *oldNode = &old_pool[j];  
        
        // Skip pruned nodes
        if (oldNode->isPruned) {
            continue;
        }

        #ifdef DEBUG_PRINT        
        printf("\n******************** Processing OLD node ************");
        printNode(oldNode, block, block_index);
        printf("\n******************************************************");
        #endif
        
        // =====================================================
        // 1. Create non-compressed node (sequence extension)
        // =====================================================
        {
            TreeNode newNode = {0}; // Fully zero-initialized
            
            // Calculate new weight (capped at SEQ_LENGTH_LIMIT-1)
            uint8_t new_weight = oldNode->incoming_weight + 1;
            if (new_weight >= SEQ_LENGTH_LIMIT) {
                new_weight = SEQ_LENGTH_LIMIT - 1;
            }
            
            // Calculate overall savings - headerOverhead. It can be negative.
            long new_saving = (calculateSavings(&block[block_index], 1, oldNode->map) + oldNode->saving_so_far) - oldNode->headerOverhead;

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
                newNode.headerOverhead = oldNode->headerOverhead;
               
                // Copy node's data to the new node. 
                copyNodeData(oldNode, &newNode, oldNode->compress_sequence_count, new_saving);

                // Add new uncompressed byte (length=1)
                if (newNode.compress_sequence_count < COMPRESS_SEQUENCE_LENGTH) {
                    newNode.compress_sequence[newNode.compress_sequence_count++] = 1;
                } else {
                    fprintf(stderr, "\n\n\n Error: newNode.compress_sequence_count(%d) >= COMPRESS_SEQUENCE_LENGTH (%d)\n", 
                        newNode.compress_sequence_count, COMPRESS_SEQUENCE_LENGTH);
                }

                // Update node map
                BinSeqKey key = create_binseq_key(&block[block_index], 1);
                BinSeqValue* value = binseq_map_get(newNode.map, key);
                
                if (value) { // If value exists (already in map)
                    if (!updateMapValue(value, block_index, 1, &newNode.headerOverhead)) {
                        fprintf(stderr, "Failed to append location to existing sequence\n");
                    }
                } else { // New sequence
                    uint32_t locs[1] = {block_index};
                    BinSeqValue newValue = create_binseq_value(1, locs, 1);
                    if (!binseq_map_put(newNode.map, key, newValue)) {
                        fprintf(stderr, "Unable to update the map\n");
                        free_key(key);
                        continue;
                    }
                }
                free_key(key);

                // Store in pool
                memcpy(&new_pool[new_nodes_count], &newNode, sizeof(TreeNode));
               
                #ifdef DEBUG_PRINT
                printNode(&new_pool[new_nodes_count], block, block_index);
                #endif
                
                new_nodes_count++;
            }
        }

        // =====================================================
        // 2. Create compressed nodes (if valid sequences exist)
        // =====================================================
        for (int k = 0; k < oldNode->incoming_weight; k++) {
            uint16_t seq_len = oldNode->incoming_weight + 1 - k;
            
            // Validate sequence length
            if (seq_len < SEQ_LENGTH_START || seq_len > SEQ_LENGTH_LIMIT) {
                continue;
            }
            
            // Check if we have enough bytes left in block
            if (block_index + 1 < seq_len) {
                continue;
            }
            
            // Create Binary Sequence
            uint8_t* seq_start = &block[block_index + 1 - seq_len];
            BinSeqKey key = create_binseq_key(seq_start, seq_len);
            
            // Calculate new savings (in bits)
            long new_saving = (calculateSavings(seq_start, seq_len, oldNode->map) + oldNode->saving_so_far) - oldNode->headerOverhead; 

            // Skip if not better than current best
            if (new_saving <= best_saving[0]) { // In this case our weight will be always zero
                free_key(key);
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

            // Configure new node
            newNode.incoming_weight = 0; // Reset weight after compression
            newNode.headerOverhead = oldNode->headerOverhead;

            // Copy relevant compression history
            int to_copy = oldNode->compress_sequence_count - oldNode->incoming_weight + k;
            if (to_copy > 0 && to_copy <= COMPRESS_SEQUENCE_LENGTH) {
    	        copyNodeData(oldNode, &newNode, to_copy, new_saving);
            }

            // Add new compressed sequence
            if (newNode.compress_sequence_count < COMPRESS_SEQUENCE_LENGTH) {
                newNode.compress_sequence[newNode.compress_sequence_count++] = seq_len;
            } else {
                fprintf(stderr, "\n\n\n Error: newNode.compress_sequence_count(%d) >= COMPRESS_SEQUENCE_LENGTH (%d)\n", 
                    newNode.compress_sequence_count, COMPRESS_SEQUENCE_LENGTH);
            }

            // Update node map
            BinSeqValue* value = binseq_map_get(newNode.map, key);
            if (value) { // If value exists (already in map)
                if (!updateMapValue(value, block_index, seq_len, &newNode.headerOverhead)) {
                    fprintf(stderr, "Failed to append location to existing sequence\n");
                }
            } else { // New sequence
                uint32_t locs[1] = {block_index + 1 - seq_len};
                BinSeqValue newValue = create_binseq_value(1, locs, 1);
                if (!binseq_map_put(newNode.map, key, newValue)) {
                    fprintf(stderr, "Unable to update the map\n");
                    free_key(key);
                    continue;
                }
            }
            free_key(key);

            // Store in pool
            memcpy(&new_pool[new_nodes_count], &newNode, sizeof(TreeNode));
            
            #ifdef DEBUG_PRINT
            printNode(&new_pool[new_nodes_count], block, block_index);
            printf("to copy = %d-%d+%d=%d", oldNode->compress_sequence_count, oldNode->incoming_weight, k, to_copy);
            #endif
            
            new_nodes_count++;
        }
    }

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

static inline void createRoot(uint8_t* block, long savings, int block_index) {
    printf("\nCreate root ");
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
    root.map = binseq_map_create(1); //create map of root.
    if (!root.map) {
        fprintf(stderr, "Error: Failed to create map for root node\n");
        return;
    }
    
    root.saving_so_far = calculateSavings(&block[0], 1, root.map);
    root.incoming_weight = 1;
    
	uint32_t locs[1] = {0};
	BinSeqKey key = create_binseq_key(&block[0], 1);
	BinSeqValue value = create_binseq_value(1, locs, 1);

	if (!binseq_map_put(root.map, key, value)) {
		fprintf(stderr, "Unable to update the map\n");
		free_key(key);  // cleanup if put fails
		free_binseq_value(value);
		return;
	}

    
    // Copy to pool
    memcpy(&node_pool_even[0], &root, sizeof(TreeNode));
    even_pool_count = 1;
    odd_pool_count = 0;
    printf("\n End copying the pool. Going to print the node ");
    printf("\n\n\n*************** new root created ********************");
    printNode(&node_pool_even[0], block, block_index);    
    printf("\n*******************************************************************");
}


static inline void resetToBestNode(TreeNode* source_pool, int source_count, uint8_t* block, uint32_t block_index) {
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
    
    //writeCompressedOutput("compress.bin", topSeq, MAX_NUMBER_OF_SEQUENCES, 
      //                   &source_pool[best_index], block);
                          
	//createRoot(block, max_saving, block_index);  
	//todo this will be gone later on. Just for testing keep it here time being.
	exit(1);
}


void processBlockSecondPass(uint8_t* block, long blockSize) {
    if (SEQ_LENGTH_LIMIT <= 1 || blockSize <= 0 || block == NULL) {
        fprintf(stderr, "Error: Invalid parameters in processBlockSecondPass\n");
        return;
    }
    createRoot(block, 0, 0);  // Start with fresh root
    uint8_t isEven = 0;  // We create root at even, and then switch to odd.
    if(1) return;

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
    if (!restedOnce) {
		if (isEven) { //valid data is in odd pool as even pool to be process next.
    	    resetToBestNode(node_pool_odd, odd_pool_count, block, blockIndex);
    	} else { //valid data is in even pool
    	    resetToBestNode(node_pool_even, even_pool_count, block, blockIndex);
    	}
    }
}

void processSecondPass(const char* filename) {
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

        processBlockSecondPass(block, bytesRead);
    }
    //printf("\n\n\n Potential Savings = %u\n", best_saving_overall);

    free(block);
    fclose(file);
}
