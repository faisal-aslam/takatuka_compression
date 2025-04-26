// src/second_pass/second_pass.c

#include "second_pass.h"
#include "tree_node_pool.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "../write_in_file/write_in_file.h"
#include "../weighted_freq.h"

#define NOT_PRUNE
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

// Global pool manager instance
TreeNodePoolManager pool_manager = {0};

uint16_t total_codes = 0;

void print_stacktrace(void);

static int processNodePath(TreeNode *oldNode, TreeNodePoolManager* mgr, int new_nodes_count,
                         const uint8_t* block, uint32_t block_size, uint32_t block_index,
                         int best_index[], int32_t best_saving[],
                         const uint8_t* sequence, uint16_t seq_len, uint8_t new_weight,
                         int to_copy);
static int createNodes(TreeNodePoolManager* mgr, int old_node_count,
                     const uint8_t* block, uint32_t block_size, uint32_t block_index);
static int validate_block_access(const uint8_t* block, uint32_t block_size, uint32_t offset, uint16_t length);

static inline uint8_t getCurrentGroup() {
    if (total_codes < getGroupThreshold(0)) {
        return 0;
    } else if (total_codes < getGroupThreshold(1)) {
        return 1;
    } else if (total_codes < getGroupThreshold(2)) {
        return 2;
    } else if (total_codes < getGroupThreshold(3)) {
        return 3;
    } else {
        fprintf(stderr, "\n Out of codes \n");
        exit(EXIT_FAILURE);
    } 
}

static inline int32_t totalSavings(TreeNode *node) {
    if (!node) return INT_MIN;  // Return minimum value for invalid node
    
    // Calculate net savings (could be positive or negative)
    int32_t net_savings = node->saving_so_far - node->headerOverhead;
    
    // Clamp to valid range
    if (net_savings > INT_MAX) return INT_MAX;
    if (net_savings < INT_MIN) return INT_MIN;
    
    return net_savings;
}

static void printNode(TreeNode *node, const uint8_t* block, uint32_t block_index) {
    if (!node) {
        printf("[NULL NODE]\n");
        return;
    }
    
    printf("\n Node :");
    printf("saving_so_far=%d, headerOverhead=%d, incoming_weight=%d, isPruned=%d, compress_seq_count=%d", 
           node->saving_so_far, 
           node->headerOverhead,
           node->incoming_weight, 
           node->isPruned, 
           (node->compress_sequence_count < COMPRESS_SEQUENCE_LENGTH) ? node->compress_sequence_count : 0);
    printf(", total Savings =%d", totalSavings(node));
    
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
    
    if (node->map) {
        binseq_map_print(node->map);
    } else {
        printf("\nMap is NULL\n");
    }
}

void cleanup_node_pools() {
    for (int i = 0; i < 2; i++) {
        TreeNodePool* pool = &pool_manager.pool[i];
        if (pool->data) {
            for (size_t j = 0; j < pool->size; j++) {
                if (pool->data[j].map) {
                    binseq_map_free(pool->data[j].map);
                    pool->data[j].map = NULL;
                }
            }
            free(pool->data);
            pool->data = NULL;
        }
        pool->size = 0;
        pool->capacity = 0;
    }
    // Don't memset the whole manager as it may contain other needed state
    pool_manager.active_index = 0;
}


static int32_t calculateSavings(const uint8_t* newBinSeq, uint16_t seq_length, BinSeqMap* map) {
    if (!newBinSeq || seq_length <= 0 || seq_length > COMPRESS_SEQUENCE_LENGTH) {
        fprintf(stderr, "Error: Invalid parameters in calculateSavings\n");
        return INT_MIN;  // Return minimum value to indicate invalid case
    }
    
    //at the moment I do not support seq_length 1 compression this might change in future.
    if (seq_length == 1) {
        return -(int32_t)(seq_length);
    }    
    
    uint8_t group = getCurrentGroup();
    //todo total_codes++;
    int32_t originalSizeBits = (int32_t)(seq_length * 8);
    int32_t compressedSizeBits = (int32_t)(groupCodeSize(group) + groupOverHead(group))-(seq_length-1);
    int32_t savings = originalSizeBits - compressedSizeBits;
    
    // Ensure we don't overflow when converting to int32_t
    if (savings > INT_MAX) return INT_MAX;
    if (savings < INT_MIN) return INT_MIN;
    
    return savings;
}

static int updateMapValue(TreeNode *node, const uint8_t* sequence, uint16_t seq_len, uint32_t location) {
    BinSeqMap* map = node->map;
    if (!map || !sequence || seq_len == 0) {
        fprintf(stderr, "\n unable to update map value ");
        return 0;
    }
    
    if (seq_len == 1) {
        return 1;// do not update map value of seq_len of 1.
    }  

    uint8_t group = getCurrentGroup();
    // Increment frequency for this sequence
    if (!binseq_map_increment_frequency(map, sequence, seq_len)) {
        /*
        *  If entry doesn't exist, 
        *  ** update header overhead of compressed sequences.
        *  ** create it with frequency 1
        */
        //only update header for the first inclusion in the map.
        node->headerOverhead += getHeaderOverhead(group, seq_len);
        
        
        return binseq_map_put(map, sequence, seq_len, 1);
    }
       
    return 1;
}

static inline void copySequences(TreeNode *sourceNode, TreeNode *destNode, uint16_t bytes_to_copy) {
    if (!sourceNode || !destNode || bytes_to_copy == 0 || 
        bytes_to_copy > COMPRESS_SEQUENCE_LENGTH || 
        bytes_to_copy > sourceNode->compress_sequence_count) {
        fprintf(stderr, "\nInvalid copy parameters\n");
        return;
    }
    
    memcpy(destNode->compress_sequence, 
           sourceNode->compress_sequence, 
           bytes_to_copy * sizeof(sourceNode->compress_sequence[0])); 
    destNode->compress_sequence_count = bytes_to_copy;
}

static BinarySequence* isValidSequence(uint16_t sequence_length, const uint8_t* block, uint32_t block_index) {
    if (sequence_length < SEQ_LENGTH_START || sequence_length > SEQ_LENGTH_LIMIT || 
        block_index < sequence_length) {
        return NULL;
    }

    uint8_t lastSequence[sequence_length];
    memcpy(lastSequence, block + (block_index - sequence_length), sequence_length);
    return lookupSequence(lastSequence, sequence_length);
}

static int createNodes(TreeNodePoolManager* mgr, int old_node_count,
    const uint8_t* block, uint32_t block_size, uint32_t block_index) {

    if (!mgr || !block || old_node_count < 0) {
        return 0;
    }

    TreeNodePool* old_pool = &mgr->pool[mgr->active_index ^ 1]; // Get inactive pool
    TreeNodePool* new_pool = &mgr->pool[mgr->active_index];

    if (old_node_count > old_pool->size) {
        fprintf(stderr, "Error: old_node_count (%d) exceeds pool size (%zu)\n", 
        old_node_count, old_pool->size);
        return 0;
    }

       
    int best_index[SEQ_LENGTH_LIMIT+1];
    int32_t best_saving[SEQ_LENGTH_LIMIT+1];
    
    for (int i = 0; i < SEQ_LENGTH_LIMIT+1; i++) {
        best_saving[i] = INT_MIN;
        best_index[i] = -1;
    }

    int new_nodes_count = 0;
    
    for (int j = 0; j < old_node_count; j++) {
        TreeNode *oldNode = &old_pool->data[j];
        #ifdef PRUNE
        if (oldNode->isPruned) continue;
        #endif

		#ifdef DEBUG
		printf("\n******************** old node to process ***************** \n");
		printNode(oldNode, block, block_index);
        printf("\n********************************************************** \n");
		#endif

	
		#ifdef DEBUG
		printf("\n uncompressed path \n");
		#endif
        // Process uncompressed path
        new_nodes_count = processNodePath(oldNode, mgr, new_nodes_count,
                                        block, block_size, block_index, best_index, best_saving,
                                        &block[block_index], 1, oldNode->incoming_weight + 1,
                                        oldNode->compress_sequence_count);
        
        // Process compressed paths
        for (int k = 0; k < oldNode->incoming_weight; k++) {
            uint16_t seq_len = oldNode->incoming_weight + 1 - k;
			#ifdef DEBUG
			printf("\n Compressed path \n");
			#endif

            
            if (seq_len < SEQ_LENGTH_START || seq_len > SEQ_LENGTH_LIMIT || 
                seq_len > (block_index + 1) || (block_index + 1 - seq_len) >= block_size) {
                
                continue;
            }
            
            const uint8_t* seq_start = &block[block_index + 1 - seq_len];
            new_nodes_count = processNodePath(oldNode, mgr, new_nodes_count,
                                            block, block_size, block_index, best_index, best_saving,
                                            seq_start, seq_len, 0,
                                            oldNode->compress_sequence_count - oldNode->incoming_weight + k);
        }
    }

    return new_nodes_count;
}

static int processNodePath(TreeNode *oldNode, TreeNodePoolManager* mgr, int new_nodes_count,
    const uint8_t* block, uint32_t block_size, uint32_t block_index,
    int best_index[], int32_t best_saving[],
    const uint8_t* sequence, uint16_t seq_len, uint8_t new_weight,
    int to_copy) {
    
    // Validations
    if (!oldNode || !mgr || !block || block_index >= block_size ||
	    block_index >= block_size || seq_len == 0 || seq_len > COMPRESS_SEQUENCE_LENGTH) {
    	fprintf(stderr,"\n processNodePath validation failed \n");
        return new_nodes_count;
    }
    
        
    uint32_t seq_start_offset = (new_weight == 0) ? block_index + 1 - seq_len : block_index;
    if (seq_start_offset >= block_size || (seq_start_offset + seq_len) > block_size) {
    	fprintf(stderr,"\n invalid seq_start \n");
        return new_nodes_count;
    }
    
    
    int32_t new_saving = calculateSavings(sequence, seq_len, oldNode);
    if (new_saving == INT_MIN) {
        // Invalid case, skip this path
        return new_nodes_count;
    }
    
    new_saving += oldNode->saving_so_far;
    uint8_t isPruned = 0;
        
    // Only prune if we have a better alternative (even if savings are negative)
    if (best_index[new_weight] != -1 && 
        new_saving - oldNode->headerOverhead <= best_saving[new_weight]) {
        isPruned = 1;
    }


    TreeNode* newNode = alloc_tree_node(mgr);
    if (!newNode) {
        fprintf(stderr,"\n node allocation failed \n");
        return new_nodes_count;
    }

    // Initialize new node (don't zero it since we may overwrite needed fields)
    newNode->incoming_weight = (new_weight >= SEQ_LENGTH_LIMIT) ? SEQ_LENGTH_LIMIT - 1 : new_weight;
    newNode->headerOverhead = oldNode->headerOverhead;
    newNode->saving_so_far = new_saving;
    newNode->isPruned = isPruned;
    newNode->compress_sequence_count = 0;  // Explicitly initialize
    
    // Copy sequences if needed
    if (to_copy > 0 && to_copy < COMPRESS_SEQUENCE_LENGTH) {
        copySequences(oldNode, newNode, to_copy);
    }
 
    // Add the new sequence
    if (newNode->compress_sequence_count >= COMPRESS_SEQUENCE_LENGTH) {
        fprintf(stderr,"\n seq count is larger than limit \n");
        binseq_map_free(newNode->map);
        newNode->map = NULL;
        return new_nodes_count;
    }

    newNode->compress_sequence[newNode->compress_sequence_count++] = seq_len;

    // copy function also creates map of the new node.
    if (!binseq_map_copy_to_node(oldNode->map, newNode)) {
        binseq_map_free(newNode->map);
        newNode->map = NULL;
        fprintf(stderr,"\n map copy failed \n");
        return new_nodes_count;
    }
    
    // Update the map with new location (only if seq_len is ge 1)
    if (seq_len > 1 && !updateMapValue(newNode, sequence, seq_len, 
                    (new_weight == 0) ? block_index + 1 - seq_len : block_index)) {
        fprintf(stderr,"\n Update node map failed \n");
        newNode->map = NULL;
        return new_nodes_count;
    }


    // Only free previous node's map if we're replacing it
    int weight_index = (new_weight == 0) ? 0 : new_weight;
    #ifdef PRUNE
    if (best_index[weight_index] != -1) {
        TreeNodePool* pool = &mgr->pool[mgr->active_index];
        TreeNode* prev_node = &pool->data[best_index[weight_index]];
        if (prev_node->map) {
            binseq_map_free(prev_node->map);
            prev_node->map = NULL;
        }
        prev_node->isPruned = 1;
    }
    #endif

    best_saving[weight_index] = new_saving - oldNode->headerOverhead;
    best_index[weight_index] = new_nodes_count;

    #ifdef DEBUG
    printf("\n new node created \n");
    printNode(newNode, block, block_index);
    #endif


    return new_nodes_count + 1;
}



static inline void createRoot(const uint8_t* block, uint32_t block_size) {
    if (!block || block_size == 0) {
        return;
    }

    cleanup_node_pools();

    TreeNode* root = alloc_tree_node(&pool_manager);
    if (!root) {
        fprintf(stderr, "FATAL: Failed to allocate root node\n");
        exit(EXIT_FAILURE);
    }

    // Initialize the node
    memset(root, 0, sizeof(TreeNode));
    root->headerOverhead = 4;
    root->compress_sequence[0] = 1;
    root->compress_sequence_count = 1;
    root->incoming_weight = 1;

    // Create empty map (won't add the single byte sequence)
    root->map = binseq_map_create(3);
    if (!root->map) {
        fprintf(stderr, "\n Unable to create map");
        free(root);  
        exit(EXIT_FAILURE);
    }

    root->saving_so_far = calculateSavings(&block[0], 1, root->map);
    
    #ifdef DEBUG
    printf("\nroot is created\n");
    printNode(root, block, 0);
    #endif
}

static inline void resetToBestNode(TreeNodePoolManager* mgr, int node_count, const uint8_t* block, uint32_t block_index) {
    int32_t max_saving = INT_MIN;
    int best_index = 0;
    TreeNodePool* pool = &mgr->pool[mgr->active_index];
    #ifdef DEBUG
    printf("\n\n --------------- Best path ------------------node_count=%d \n", node_count);
    #endif

    
    for (int i = 0; i < node_count; i++) {
        #ifdef DEBUG
        printNode(&pool->data[i], block, block_index);
        #endif

        if (totalSavings(&pool->data[i]) > max_saving) {
            max_saving = totalSavings(&pool->data[i]);
            best_index = i;
        }
    }
    #ifdef DEBUG
    printNode(&pool->data[best_index], block, block_index);
    #endif
    
    // Write compressed output here if needed
    // writeCompressedOutput("output.bin", ...);
    
    cleanup_node_pools();
    exit(0);  // Use exit(0) instead of exit(1) for normal termination
}

void processBlockSecondPass(const uint8_t* block, uint32_t block_size) {
    if (SEQ_LENGTH_LIMIT <= 1 || block_size <= 0 || block == NULL) {
        fprintf(stderr, "Error: Invalid parameters in processBlockSecondPass\n");
        cleanup_node_pools();
        return;
    }
    
    createRoot(block, block_size);
    uint8_t isEven = 0;
    uint32_t blockIndex;    
    
    for (blockIndex = 1; blockIndex < block_size; blockIndex++) {
        if (blockIndex % COMPRESS_SEQUENCE_LENGTH == 0) {
            
            resetToBestNode(&pool_manager, 
                          pool_manager.pool[pool_manager.active_index].size,
                          block, blockIndex);
            isEven = 0;
        }

        // Switch pools and get the correct size
        switch_tree_node_pool(&pool_manager);
        int old_pool_size = pool_manager.pool[pool_manager.active_index ^ 1].size;
        
        if (isEven) {
            createNodes(&pool_manager, old_pool_size, block, block_size, blockIndex);
        } else {
            createNodes(&pool_manager, old_pool_size, block, block_size, blockIndex);
        }
        
        isEven = !isEven;
    }
    
    resetToBestNode(&pool_manager, pool_manager.pool[pool_manager.active_index].size, block, blockIndex);
    
    cleanup_node_pools();
}

void processSecondPass(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    // Initialize pool manager first
    init_tree_node_pool_manager(&pool_manager);

    uint8_t* block = malloc(BLOCK_SIZE);
    if (!block) {
        perror("Failed to allocate memory for block");
        fclose(file);
        cleanup_node_pools();
        return;
    }

    while (1) {
        long bytesRead = fread(block, 1, BLOCK_SIZE, file);
        if (bytesRead <= 0) break;
      
        processBlockSecondPass(block, bytesRead);
    }
    
    free(block);
    fclose(file);
    cleanup_node_pools();
}
