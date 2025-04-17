// src/second_pass/second_pass.c
#include "second_pass.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "../write_in_file/write_in_file.h"
#include "../weighted_freq.h"

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
static int validate_block_access(const uint8_t* block, uint32_t block_size, uint32_t offset, uint16_t length);

static inline int32_t totalSavings(TreeNode *node) {
    if (!node) return 0;
    return (node->saving_so_far - node->headerOverhead);
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

static int32_t calculateSavings(const uint8_t* newBinSeq, uint16_t seq_length, BinSeqMap *map) {
    if (!newBinSeq || seq_length <= 0 || seq_length > COMPRESS_SEQUENCE_LENGTH || !map) {
        fprintf(stderr, "Error: Invalid parameters in calculateSavings\n");
        return 0;
    }
    
    const int* freq = binseq_map_get_frequency(map, newBinSeq, seq_length);
    if (!freq || seq_length == 1) {
        return -(long)(seq_length);
    }
    
    int group = (seq_length == 1) ? 0 : 3;
    long originalSizeBits = (long)(seq_length * 8);
    long compressedSizeBits = (long)(groupCodeSize(group) + groupOverHead(group));
    long savings = originalSizeBits - compressedSizeBits;
    
    if (savings < 0) {
        fprintf(stderr, "\nSavings is less than zero after compression \n");
        return 0;
    }
    
    return savings;
}

static int updateMapValue(BinSeqMap* map, const uint8_t* sequence, uint16_t seq_len, uint32_t location) {
    if (!map || !sequence || seq_len == 0) return 0;
    
    // Try to append to existing entry
    if (binseq_map_append_location(map, sequence, seq_len, location)) {
        return 1;
    }
    
    // If append failed, create new entry
    uint32_t locs[1] = {location};
    if (binseq_map_put(map, sequence, seq_len, 1, locs, 1)) {
        return 1;
    }
    
    return 0;
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

static int createNodes(TreeNode *old_pool, TreeNode *new_pool, int old_node_count,
                     const uint8_t* block, uint32_t block_size, uint32_t block_index) {
    if (!old_pool || !new_pool || !block || old_node_count < 0) {
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
        TreeNode *oldNode = &old_pool[j];
        if (oldNode->isPruned) continue;

        // Process uncompressed path
        new_nodes_count = processNodePath(oldNode, new_pool, new_nodes_count,
                                        block, block_size, block_index, best_index, best_saving,
                                        &block[block_index], 1, oldNode->incoming_weight + 1,
                                        oldNode->compress_sequence_count);
        
        // Process compressed paths
        for (int k = 0; k < oldNode->incoming_weight; k++) {
            uint16_t seq_len = oldNode->incoming_weight + 1 - k;
            
            if (seq_len < SEQ_LENGTH_START || seq_len > SEQ_LENGTH_LIMIT || 
                seq_len > (block_index + 1) || (block_index + 1 - seq_len) >= block_size) {
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

static int updateNodeMap(TreeNode *node, const uint8_t* sequence, uint16_t seq_len, uint32_t location) {
    if (!node || !node->map || !sequence || seq_len == 0 || location >= BLOCK_SIZE || 
        (location + seq_len) > BLOCK_SIZE) {
        return 0;
    }
    
    if (!updateMapValue(node->map, sequence, seq_len, location)) {
        return 0;
    }
    
    if (seq_len > 1) {
        node->headerOverhead += getHeaderOverhead((seq_len == 1) ? 0 : 3, seq_len);
    }
    
    return 1;
}

static int processNodePath(TreeNode *oldNode, TreeNode *new_pool, int new_nodes_count,
                         const uint8_t* block, uint32_t block_size, uint32_t block_index,
                         int best_index[], int32_t best_saving[],
                         const uint8_t* sequence, uint16_t seq_len, uint8_t new_weight,
                         int to_copy) {
    if (block_index >= block_size || seq_len == 0 || seq_len > COMPRESS_SEQUENCE_LENGTH) {
        return new_nodes_count;
    }
    
    uint32_t seq_start_offset = (new_weight == 0) ? block_index + 1 - seq_len : block_index;
    if (seq_start_offset >= block_size || (seq_start_offset + seq_len) > block_size) {
        return new_nodes_count;
    }
    
    int32_t new_saving = calculateSavings(sequence, seq_len, oldNode->map) + oldNode->saving_so_far;
    uint8_t isPruned = 0;
    
    if (!(best_index[new_weight] == -1 || new_saving - oldNode->headerOverhead <= best_saving[new_weight])) {
        isPruned = 1;
    }

    TreeNode newNode = {0};
    newNode.incoming_weight = (new_weight >= SEQ_LENGTH_LIMIT) ? SEQ_LENGTH_LIMIT - 1 : new_weight;
    newNode.headerOverhead = oldNode->headerOverhead;
    newNode.saving_so_far = new_saving;
    newNode.isPruned = isPruned;

    if (to_copy > 0 && to_copy < COMPRESS_SEQUENCE_LENGTH) {
        copySequences(oldNode, &newNode, to_copy);
    }

    // Create new map for the node
    newNode.map = binseq_map_create(binseq_map_capacity(oldNode->map) + 1);
    if (!newNode.map) {
        return new_nodes_count;
    }
    binseq_map_copy_to_node(oldNode->map, &newNode);

    if (newNode.compress_sequence_count >= COMPRESS_SEQUENCE_LENGTH) {
        binseq_map_free(newNode.map);
        return new_nodes_count;
    }
    newNode.compress_sequence[newNode.compress_sequence_count++] = seq_len;

    if (!updateNodeMap(&newNode, sequence, seq_len, 
                      (new_weight == 0) ? block_index + 1 - seq_len : block_index)) {
        binseq_map_free(newNode.map);
        return new_nodes_count;
    }

    int weight_index = (new_weight == 0) ? 0 : new_weight;
    if (best_index[weight_index] != -1) {
        new_pool[best_index[weight_index]].isPruned = 1;
    }
    
    best_saving[weight_index] = new_saving - oldNode->headerOverhead;
    best_index[weight_index] = new_nodes_count;

    if (new_pool[new_nodes_count].map) {
        binseq_map_free(new_pool[new_nodes_count].map);
    }

    memcpy(&new_pool[new_nodes_count], &newNode, sizeof(TreeNode));
    new_pool[new_nodes_count].map = newNode.map;
    newNode.map = NULL;

    return new_nodes_count + 1;
}

static inline void createRoot(const uint8_t* block, uint32_t block_size) {
    if (!block || block_size == 0) {
        return;
    }

    cleanup_node_pools();
    memset(node_pool_even, 0, sizeof(node_pool_even));
    memset(node_pool_odd, 0, sizeof(node_pool_odd));

    TreeNode root = {0};
    root.headerOverhead = 4;
    root.compress_sequence[0] = 1;
    root.compress_sequence_count = 1;
    root.incoming_weight = 1;

    root.map = binseq_map_create(16);
    if (!root.map) {
        fprintf(stderr, "FATAL: Failed to create root map\n");
        exit(EXIT_FAILURE);
    }

    uint32_t locs[1] = {0};
    if (!binseq_map_put(root.map, &block[0], 1, 1, locs, 1)) {
        binseq_map_free(root.map);
        return;
    }

    root.saving_so_far = calculateSavings(&block[0], 1, root.map);

    if (node_pool_even[0].map) {
        binseq_map_free(node_pool_even[0].map);
    }
    node_pool_even[0] = root;
    root.map = NULL;

    even_pool_count = 1;
    odd_pool_count = 0;
}

static inline void resetToBestNode(TreeNode* source_pool, int source_count, const uint8_t* block, uint32_t block_index) {
    int32_t max_saving = INT_MIN;
    int best_index = 0;
    
    for (int i = 0; i < source_count; i++) {
        if (totalSavings(&source_pool[i]) > max_saving) {
            max_saving = totalSavings(&source_pool[i]);
            best_index = i;
        }
    }
    
    cleanup_node_pools();
    exit(1);
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
    uint8_t restedOnce = 0;
    
    for (blockIndex = 1; blockIndex < block_size; blockIndex++) {
        if (blockIndex % COMPRESS_SEQUENCE_LENGTH == 0) {
            restedOnce = 1;
            if (isEven) {
                resetToBestNode(node_pool_odd, odd_pool_count, block, blockIndex);
            } else {
                resetToBestNode(node_pool_even, even_pool_count, block, blockIndex);
            }
            isEven = 0;
        }

        if (isEven) {
            even_pool_count = createNodes(node_pool_odd, node_pool_even, 
                                        odd_pool_count, block, block_size, blockIndex);
        } else {
            odd_pool_count = createNodes(node_pool_even, node_pool_odd,
                                       even_pool_count, block, block_size, blockIndex);
        }
        
        isEven = !isEven;
    }
    
    if (!restedOnce) {
        if (isEven) {
            resetToBestNode(node_pool_odd, odd_pool_count, block, blockIndex);
        } else {
            resetToBestNode(node_pool_even, even_pool_count, block, blockIndex);
        }
    }
    
    cleanup_node_pools();
}

void processSecondPass(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        cleanup_node_pools(); 
        return;
    }

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