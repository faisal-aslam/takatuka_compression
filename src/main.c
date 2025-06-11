// src/main.c

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
            
    uint8_t weight = (new_weight >= SEQ_LENGTH_LIMIT) ? SEQ_LENGTH_LIMIT - 1 : new_weight;
    GraphNode* new_node = create_new_node(weight, old_node->level+1);
    if (!new_node) {
        fprintf(stderr,"\n node allocation failed \n");
        return;
    }

    //add an edge.
    graph_add_edge(old_node, new_node);

    new_node->incoming_weight = weight;
    new_node->saving_so_far = new_saving;
    new_node->compress_sequence = 0;
    new_node->level = old_node->level+1;
    new_node->compress_sequence = seq_len;

    
    #ifdef DEBUG
    //printf("\n\n new node created \n");
    print_graph_node(new_node, block);
    #endif

}


/**
 * Create root of the graph
 * @block bytes of the block read from the file.
 * @block_size the size of the array block.
*/
static inline void createRoot(const uint8_t* block, uint32_t block_size) {
    /**
     * Return if the block is null or empty.
     * This only happens when we have reached the end of file. 
    */ 
    if (!block || block_size == 0) {
        return;
    }

    // initialize the graph.
    graph_init();
    // Ensure pool has capacity
    if (is_graph_full()) {
        fprintf(stderr, "FATAL: Graph is full\n");
        exit(EXIT_FAILURE);
    }

    // create root node and set its values
    GraphNode* root = create_new_node(1, 1);
    
    root->incoming_weight = 1; //root weight must be 1.
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

    for (uint32_t block_index = 1; block_index < block_size; block_index++) {
        uint32_t current_level = get_max_level();
        
        // Initialize weight cache to "not found" state
        for (int w = 0; w < MAX_WEIGHT; w++) {
            graph.weight_cache[w].first_node_with_weight = UINT32_MAX;
            graph.weight_cache[w].weight = 0;
        }
        
        // Process all nodes at current level
        for (uint8_t weight = 1; weight <= MAX_WEIGHT; weight++) {
            uint32_t node_count = 0;
            const uint32_t* node_indices = get_nodes_by_weight_and_level(weight, current_level, &node_count);
            
            if (node_count == 0) continue;

            for (uint32_t i = 0; i < node_count; i++) {
                GraphNode *old_node = graph_get_node(node_indices[i]);
                if (!old_node) continue;

                // Check if we've already processed this weight
                if (graph.weight_cache[weight].first_node_with_weight != UINT32_MAX && 
                    graph.weight_cache[weight].first_node_with_weight != node_indices[i]) 
                {
                    // Reuse children from first node with same weight
                    GraphNode *first_node = graph_get_node(graph.weight_cache[weight].first_node_with_weight);
                    
                    // Link to existing children
                    for (uint8_t c = 0; c < first_node->child_count; c++) {
                        graph_add_edge(old_node, graph_get_node(first_node->children[c]));
                    }
                    continue;
                }

                // Only set first_node_with_weight if not already set
                if (graph.weight_cache[weight].first_node_with_weight == UINT32_MAX) {
                    graph.weight_cache[weight].first_node_with_weight = node_indices[i];
                    graph.weight_cache[weight].weight = weight;
                }

                // Process paths only for first node with this weight
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
                    processNodePath(old_node, block, block_size, block_index, 
                                  seq_start, seq_len, 0);
                }

#ifdef DEBUG
                graphviz_add_level(&viz, i, node_count, block);
#endif
            }
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
        //process of block of file at a time.
        processBlock(block, bytesRead);
    }

    free(block);
    block = NULL;

    fclose(file);

    return 0;
}
