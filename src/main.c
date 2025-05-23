// src/second_pass/second_pass.c

#include "graph/graph_visualizer.h"
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

#ifdef DEBUG
GraphVisualizer viz;
#endif

uint16_t total_codes = 0;

static void processNodePath(GraphNode *old_node, const uint8_t* block, uint32_t block_size, uint32_t block_index,    
    const uint8_t* sequence, uint8_t seq_len, uint8_t new_weight);

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



static void processNodePath(GraphNode *old_node, const uint8_t* block, uint32_t block_size, uint32_t block_index,    
    const uint8_t* sequence, uint8_t seq_len, uint8_t new_weight) {
    
    // Validations
    if (!old_node ||  !block || block_index >= block_size ||
	    block_index >= block_size || seq_len == 0) {
    	fprintf(stderr,"\n processNodePath validation failed \n");
        return;
    }
    
        
    uint32_t seq_start_offset = (new_weight == 0) ? block_index + 1 - seq_len : block_index;
    if (seq_start_offset >= block_size || (seq_start_offset + seq_len) > block_size) {
    	fprintf(stderr,"\n invalid seq_start \n");
        return;
    }
    
    
    int32_t new_saving = calculate_savings(sequence, seq_len, NULL);
    if (new_saving == INT_MIN) {
        // Invalid case, skip this path
        return;
    }
    
    new_saving += old_node->saving_so_far;
            

    GraphNode* new_node = create_new_node();
    if (!new_node) {
        fprintf(stderr,"\n node allocation failed \n");
        return;
    }

    new_node->parents[0] = old_node ? old_node->id : 0;


    new_node->incoming_weight = (new_weight >= SEQ_LENGTH_LIMIT) ? SEQ_LENGTH_LIMIT - 1 : new_weight;
    new_node->saving_so_far = new_saving;
    new_node->compress_sequence = 0;
    new_node->level = old_node->level+1;
    new_node->compress_sequence = seq_len;

    
    #ifdef DEBUG
    //printf("\n\n new node created \n");
    print_graph_node(new_node, block);
    #endif

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
    GraphNode* root = create_new_node();
    
    root->incoming_weight = 1; //root default weight is 1.
    root->parent_count = 0; //root has no parents.
    root->compress_sequence = 1; //there is nothing to compress yet at the root level.
    root->level = 1; //root is at level 1

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
    graphviz_add_level(&viz, 0, 1, block);
    #endif
  

}


static void processBlock(const uint8_t *block, uint32_t block_size) {
    if (SEQ_LENGTH_LIMIT <= 1 || block_size == 0 || !block) {
        fprintf(stderr, "Error: Invalid parameters in processBlock\n");
        return;
    }
#ifdef DEBUG
    graphviz_init(&viz, "compression_tree.dot", true);
#endif

    createRoot(block, block_size);
    //graphviz_add_level
    int level_node_count = 0;

    uint32_t block_index;
    /*
        Using each block_index use all the previous level nodes to create new
       level.
    */
    for (block_index = 1; block_index < block_size; block_index++) {
        uint32_t current_index = get_current_graph_node_index();
        GraphNode *current_node = graph_get_node(current_index);
        if (current_node == NULL) {
            fprintf(stderr, "null node encountered");
            exit(EXIT_FAILURE);
        }
        uint32_t old_node_level = graph_get_node(current_index)->level;
        level_node_count = 0;
        int end_index = current_index;
        for (int index = current_index; index >= 0; index--) {
            
            GraphNode *old_node = graph_get_node(index);
            if (old_node == NULL) {
                fprintf(stderr, "null node encountered");
                exit(EXIT_FAILURE);
            }
            if (old_node->level != old_node_level) {
                break; // done with creating new nodes of the last level.
            }
            level_node_count++;
            // Uncompressed path
            processNodePath(old_node, block, block_size, block_index,
                &block[block_index], 1, old_node->incoming_weight + 1);

            // Compressed paths
            for (int k = 0; k < old_node->incoming_weight; k++) {
                uint16_t seq_len = old_node->incoming_weight + 1 - k;

                if (seq_len < SEQ_LENGTH_START || seq_len > SEQ_LENGTH_LIMIT ||
                    seq_len > (block_index + 1) ||
                    (block_index + 1 - seq_len) >= block_size) {
                    continue;
                }
                
                const uint8_t *seq_start = &block[block_index + 1 - seq_len];
                processNodePath(old_node, block, block_size, block_index, seq_start, seq_len, 0
                    /*,old_node->compress_sequence - old_node->incoming_weight + k*/);
            }
            #ifdef DEBUG 
            int start_index = level_node_count - end_index;            
            graphviz_add_level(&viz, start_index, end_index, block);
            #endif
        }
    }
#ifdef DEBUG
    graphviz_finalize(&viz);
#endif
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        // printf("Usage: %s <input_file> <output_file>\n", argv[0]);
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        perror("Failed to open file");
        return 1;
    }

    uint8_t *block = malloc(BLOCK_SIZE);
    if (!block) {
        perror("Failed to allocate memory for block");
        fclose(file);
        return 1;
    }

    while (1) {
        long bytesRead = fread(block, 1, BLOCK_SIZE, file);
        if (bytesRead <= 0)
            break;

        processBlock(block, bytesRead);
    }

    free(block);
    block = NULL;

    fclose(file);

    return 0;
}
