// src/second_pass/second_pass.c

#include "second_pass/tree_visualizer.h"
#include "second_pass/tree_node_pool.h"
#include "second_pass/tree_node.h"
#include "second_pass/prune_logic.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include "write_in_file/write_in_file.h"
#include "second_pass/group.h"
#include "graph/graph.h"

// Global pool manager instance
TreeNodePoolManager pool_manager = {0};

TreeVisualizer viz;

uint16_t total_codes = 0;

static int processNodePath(TreeNode *old_node, TreeNodePoolManager* mgr, int new_nodes_count,
                         const uint8_t* block, uint32_t block_size, uint32_t block_index,                         
                         const uint8_t* sequence, uint16_t seq_len, uint8_t new_weight,
                         int to_copy);
static int createNodes(TreeNodePoolManager* mgr, int old_node_count,
                     const uint8_t* block, uint32_t block_size, uint32_t block_index);

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


static void cleanup_node_pools() {
    for (int i = 0; i < 2; i++) {
        TreeNodePool* pool = &pool_manager.pool[i];
        if (pool->data) {
            for (size_t j = 0; j < pool->size; j++) {
                free_tree_node(&pool->data[j]);
            }
            free(pool->data);
            pool->data = NULL;
        }
        pool->size = 0;
        pool->capacity = 0;
    }
    pool_manager.active_index = 0;
}


static int updateMapValue(TreeNode *node, const uint8_t* sequence, uint16_t seq_len) {
    BinSeqMap* map = node->map;
    if (!map || !sequence || seq_len == 0) {
        fprintf(stderr, "\n unable to update map value ");
        return 0;
    }
    
    if (seq_len == 1) {
        return 1;// do not update map value of seq_len of 1.
    }  

    //uint8_t group = getCurrentGroup();
    // Increment frequency for this sequence
    if (!binseq_map_increment_frequency(map, sequence, seq_len)) {
        /*
        *  If entry doesn't exist, 
        *  ** create it with frequency 1
        */
        return binseq_map_put(map, sequence, seq_len, 1);
    }
       
    return 1;
}


static int createNodes(TreeNodePoolManager* mgr, int old_node_count,
    const uint8_t* block, uint32_t block_size, uint32_t block_index) {

    if (!mgr || !block || old_node_count < 0) {
        return 0;
    }


    TreeNodePool* old_pool = &mgr->pool[mgr->active_index ^ 1];
    TreeNodePool* new_pool = &mgr->pool[mgr->active_index];

    if (old_node_count > (int)old_pool->size) {
        fprintf(stderr, "Error: old_node_count (%d) exceeds pool size (%zu)\n", 
        old_node_count, old_pool->size);
        return 0;
    }

    int new_nodes_count = 0;
    
    for (int j = 0; j < old_node_count; j++) {
        TreeNode *old_node = &old_pool->data[j];

        if (old_node->isPruned) continue;

        #ifdef DEBUG
        printf("\n\n *********************** Processing old Node ****************");
        print_tree_node(old_node, block, block_index);
        printf("\n\n ***********************************************************");
        #endif
        // Uncompressed path
        new_nodes_count = processNodePath(old_node, mgr, new_nodes_count,
                                          block, block_size, block_index,                                        
                                          &block[block_index], 1,
                                          old_node->incoming_weight + 1,
                                          old_node->compress_sequence_count);

        // Compressed paths
        for (int k = 0; k < old_node->incoming_weight; k++) {
            uint16_t seq_len = old_node->incoming_weight + 1 - k;

            if (seq_len < SEQ_LENGTH_START || seq_len > SEQ_LENGTH_LIMIT ||
                seq_len > (block_index + 1) || (block_index + 1 - seq_len) >= block_size) {
                continue;
            }

            const uint8_t* seq_start = &block[block_index + 1 - seq_len];
            new_nodes_count = processNodePath(old_node, mgr, new_nodes_count,
                                              block, block_size, block_index,                                              
                                              seq_start, seq_len, 0,
                                              old_node->compress_sequence_count - old_node->incoming_weight + k);
        }
        
    }

    // PRUNE step: mark nodes that are not the best as pruned
    apply_dual_beam_pruning(new_pool, new_nodes_count, block, block_index);
    // VISUALIZE pruning results
    #ifdef DEBUG 
    visualize_add_level(&viz, &new_pool->data[0], new_nodes_count, block);
    #endif
    
    return new_nodes_count;
}


static int processNodePath(TreeNode *old_node, TreeNodePoolManager* mgr, int new_nodes_count,
    const uint8_t* block, uint32_t block_size, uint32_t block_index,
    
    const uint8_t* sequence, uint16_t seq_len, uint8_t new_weight,
    int to_copy) {
    
    // Validations
    if (!old_node || !mgr || !block || block_index >= block_size ||
	    block_index >= block_size || seq_len == 0) {
    	fprintf(stderr,"\n processNodePath validation failed \n");
        return new_nodes_count;
    }
    
        
    uint32_t seq_start_offset = (new_weight == 0) ? block_index + 1 - seq_len : block_index;
    if (seq_start_offset >= block_size || (seq_start_offset + seq_len) > block_size) {
    	fprintf(stderr,"\n invalid seq_start \n");
        return new_nodes_count;
    }
    
    
    int32_t new_saving = calculate_savings(sequence, seq_len, old_node->map);
    if (new_saving == INT_MIN) {
        // Invalid case, skip this path
        return new_nodes_count;
    }
    
    new_saving += old_node->saving_so_far;
    uint8_t isPruned = 0;
        

    TreeNode* new_node = alloc_tree_node(mgr);
    if (!new_node) {
        fprintf(stderr,"\n node allocation failed \n");
        return new_nodes_count;
    }

    // Initialize with default capacity (e.g., 16)
    init_tree_node(new_node, 16);     

    new_node->id = ++viz.next_node_id;
    new_node->parent_id = old_node ? old_node->id : 0;


    new_node->incoming_weight = (new_weight >= SEQ_LENGTH_LIMIT) ? SEQ_LENGTH_LIMIT - 1 : new_weight;
    new_node->saving_so_far = new_saving;
    new_node->isPruned = isPruned;
    new_node->compress_sequence_count = 0;
    
    // Copy sequences if needed
    if (to_copy > 0) {
        copy_tree_node_sequences(old_node, new_node, to_copy);
    }
 
    // Add the new sequence
    if (!ensure_sequence_capacity(new_node, new_node->compress_sequence_count + 1)) {
        fprintf(stderr,"\n Failed to expand sequence capacity\n");
        free_tree_node(new_node);
        return new_nodes_count;
    }
    new_node->compress_sequence[new_node->compress_sequence_count++] = seq_len;

    // copy function also creates map of the new node.
    if (!binseq_map_copy_to_node(old_node->map, new_node)) {
        binseq_map_free(new_node->map);
        new_node->map = NULL;
        fprintf(stderr,"\n map copy failed \n");
        return new_nodes_count;
    }
    
    // Update the map by incrementing frequency 
    if (seq_len > 1 && !updateMapValue(new_node, sequence, seq_len)) {
        fprintf(stderr,"\n Update node map failed \n");
        new_node->map = NULL;
        return new_nodes_count;
    }

    
    #ifdef DEBUG
    //printf("\n\n new node created \n");
    print_tree_node(new_node, block, block_index);
    #endif


    return new_nodes_count + 1;
}



static inline void createRoot(const uint8_t* block, uint32_t block_size) {
    if (!block || block_size == 0) return;
    // initialize the graph.
    graph_init();
    // Ensure pool has capacity
    if (is_graph_full()) {
        fprintf(stderr, "FATAL: Graph is full\n");
        exit(EXIT_FAILURE);
    }

    // Initialize the root node directly in the pool
    GraphNode* root = get_next_node();
    
    //todo root->id = ++viz.next_node_id;
    root->incoming_weight = 1;
    root->parent_count = 0;
    root->compress_sequence = 1;

    // Create empty map
   /* todo root->map = binseq_map_create(3);
    if (!root->map) {
        fprintf(stderr, "\n Unable to create map");
        pool->size--; // Remove the failed node from pool
        free_tree_node_pool_manager(&pool_manager);
        exit(EXIT_FAILURE);
    }
    */
    root->saving_so_far = calculate_savings(&block[0], 1, NULL);

    #ifdef DEBUG
    printf("\nCreated new root node in pool[0][0]:\n");
    print_graph_node(root, block);
    //todo printf("Pool size: %zu\n", pool->size);
    //visualize_add_level(&viz, root, 1, block);
    #endif
  

}


static inline TreeNode* resetToBestNode(TreeNodePoolManager* mgr, int node_count, 
    const uint8_t* block, uint32_t block_index) {
    if (!mgr || node_count <= 0 || !block) {
        fprintf(stderr, "Invalid parameters to resetToBestNode\n");
        return NULL;
    }

    TreeNodePool* pool = &mgr->pool[mgr->active_index];
    if (!pool || !pool->data || node_count > (int)pool->size) {
        fprintf(stderr, "Invalid pool state in resetToBestNode\n");
        return NULL;
    }

    // Find the best node
    int32_t max_saving = INT_MIN;
    int best_index = -1;
    
    for (int i = 0; i < node_count; i++) {
        TreeNode* current = &pool->data[i];
        if (!current->compress_sequence) continue;

        if (current->saving_so_far > max_saving) {
            max_saving = current->saving_so_far;
            best_index = i;
        } else if (current->saving_so_far == max_saving) {
            if (current->compress_sequence_count < pool->data[best_index].compress_sequence_count) {
                best_index = i;
            }
        }
    }

    if (best_index == -1) {
        fprintf(stderr, "No valid node found in resetToBestNode\n");
        return NULL;
    }

    TreeNode* best_node = &pool->data[best_index];

    // Create a fresh new_root (heap-allocated)
    uint16_t initial_capacity = best_node->compress_sequence_count > 0 ? 
                                 best_node->compress_sequence_count : 1;

    TreeNode* new_root = create_tree_node(initial_capacity);
    new_root->id = ++viz.next_node_id;
    new_root->parent_id = 0;
    if (!new_root) {
        fprintf(stderr, "Failed to create new root node\n");
        return NULL;
    }

    // Manually copy fields
    new_root->saving_so_far = best_node->saving_so_far;
    new_root->incoming_weight = best_node->incoming_weight;
    new_root->isPruned = best_node->isPruned;
    new_root->compress_sequence_count = best_node->compress_sequence_count;

    if (best_node->compress_sequence && best_node->compress_sequence_count > 0) {
        memcpy(new_root->compress_sequence, best_node->compress_sequence,
               best_node->compress_sequence_count * sizeof(uint16_t));
    }

    if (best_node->map) {
        if (!binseq_map_copy_to_node(best_node->map, new_root)) {
            fprintf(stderr, "Failed to copy map to new root\n");
            free_tree_node(new_root);
            return NULL;
        }
    }

    printf("\n\n**************** Successfully created new root node after reset:\n");
    print_tree_node(new_root, block, block_index);    
    //todo later. writeCompressedOutput("compress.bin", NULL, MAX_NUMBER_OF_SEQUENCES, best_node, block);
    
    return new_root;
}


static void processBlockSecondPass(const uint8_t* block, uint32_t block_size) {
    if (SEQ_LENGTH_LIMIT <= 1 || block_size == 0 || !block) {
        fprintf(stderr, "Error: Invalid parameters in processBlockSecondPass\n");
        cleanup_node_pools();
        return;
    }
    #ifdef DEBUG 
    init_visualizer(&viz, "compression_tree.dot", 1);
    #endif 

    createRoot(block, block_size);

    uint32_t blockIndex;


    for (blockIndex = 1; blockIndex < block_size; blockIndex++) {
        // Switch pools
        switch_tree_node_pool(&pool_manager);
        int old_pool_size = pool_manager.pool[pool_manager.active_index ^ 1].size;

        if (!createNodes(&pool_manager, old_pool_size, block, block_size, blockIndex)) {
            fprintf(stderr, "Failed to create nodes\n");
            cleanup_node_pools();
            return;
        }

        // Reset root periodically or at the end
        if ((blockIndex + 1) % RESET_ROOT_LENGTH == 0 || (blockIndex + 1) == block_size) {
            TreeNode* new_root = resetToBestNode(&pool_manager, 
                pool_manager.pool[pool_manager.active_index].size,
                block, blockIndex);

            if (!new_root) {
                fprintf(stderr, "Failed to create new root\n");
                cleanup_node_pools();
                return;
            }

            // Insert the new root into pool
            TreeNode* root_node = alloc_tree_node(&pool_manager);
            if (!root_node) {
                fprintf(stderr, "Failed to allocate new root node in pool\n");
                free_tree_node(new_root);
                cleanup_node_pools();
                return;
            }

            // Safe field-by-field copy
            root_node->saving_so_far = new_root->saving_so_far;
            root_node->incoming_weight = new_root->incoming_weight;
            root_node->isPruned = new_root->isPruned;
            root_node->compress_sequence_count = new_root->compress_sequence_count;

            if (new_root->compress_sequence && new_root->compress_sequence_count > 0) {
                root_node->compress_sequence = malloc(new_root->compress_sequence_count * sizeof(uint16_t));
                if (!root_node->compress_sequence) {
                    fprintf(stderr, "Failed to allocate compress_sequence\n");
                    free_tree_node(new_root);
                    cleanup_node_pools();
                    return;
                }
                memcpy(root_node->compress_sequence, new_root->compress_sequence,
                       new_root->compress_sequence_count * sizeof(uint16_t));
            } else {
                root_node->compress_sequence = NULL;
            }

            if (new_root->map) {
                if (!binseq_map_copy_to_node(new_root->map, root_node)) {
                    fprintf(stderr, "Failed to copy map to new root\n");
                    if (root_node->compress_sequence) free(root_node->compress_sequence);
                    free_tree_node(new_root);
                    free(new_root);
                    new_root = NULL;
                    cleanup_node_pools();
                    return;
                }
            } else {
                root_node->map = NULL;
            }

            // Free the temporary root
            free_tree_node(new_root);
            free(new_root);
            new_root = NULL;
            
        }
    }
    #ifdef DEBUG
    finalize_visualizer(&viz);
    #endif

    cleanup_node_pools();
}


static void processSecondPass(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        return;
    }

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
    block = NULL;

    fclose(file);
    cleanup_node_pools();
}



int main(int argc, char *argv[]) {
    if (argc != 2) {
        //printf("Usage: %s <input_file> <output_file>\n", argv[0]);
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }
    
    processSecondPass(argv[1]);

    return 0;
}
