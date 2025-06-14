// main.c

#include "graph/graph_visualizer.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>
//#include "write_in_file/write_in_file.h"
#include "second_pass/group.h"
#include "graph/graph.h"
#include "second_pass/prune_logic.h"

#ifdef DEBUG
GraphVisualizer viz;
#endif

uint16_t total_codes = 0;

static void processNodePath(uint32_t old_node_index, const uint8_t* block, uint32_t block_size, uint32_t block_index,    
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



/*
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
        // If entry doesn't exist, create it with frequency 1
        return binseq_map_put(map, sequence, seq_len, 1);
    }
       
    return 1;
}
*/

static void processCompressPath(const uint8_t* block, uint32_t block_size, uint32_t block_index,    
    const uint8_t* sequence, uint8_t seq_len, uint32_t current_level) {
    
    uint32_t seq_start_offset = block_index + 1 - seq_len;
    if (seq_start_offset >= block_size || (seq_start_offset + seq_len) > block_size) {
        fprintf(stderr,"\n invalid seq_start \n");
        return;
    }
    
    int32_t new_saving = calculate_savings(sequence, seq_len, NULL);
    if (new_saving == INT_MIN) {
        return;
    }
    //todo new_saving += old_node->saving_so_far;
            
    uint8_t weight = 0;
    
    // CREATE NODE FIRST
    GraphNode* new_node = create_new_node(weight, current_level+1);
    if (!new_node) {
        fprintf(stderr,"\n node allocation failed level=%d, weight=%u\n", current_level+1, weight);
        exit(1);
        return;
    }

    // SET NODE PROPERTIES
    new_node->incoming_weight = weight;
    new_node->saving_so_far = new_saving;
    new_node->compress_sequence = seq_len;
    new_node->level = current_level+1;
    new_node->compress_start_index = seq_start_offset;
    
#ifdef DEBUG
    print_graph_node(new_node, block);
#endif
    for (uint8_t parent_weight = seq_len - 1;
         parent_weight < SEQ_LENGTH_LIMIT &&
         parent_weight <= (uint8_t)current_level;
         parent_weight++) {
        uint32_t count;
        const uint32_t *indexes = get_nodes_by_weight_and_level(parent_weight, current_level, &count);
        if (count == 0) {
            continue;
        }
        // ADD EDGE AFTER NODE IS FULLY INITIALIZED
        if (!graph_add_edge(indexes[0], new_node->id)) {
            fprintf(stderr, "Failed to add edge from %u to %u\n", indexes[0],
                    new_node->id);
            return;
        }
    }
}

static void processNodePath(uint32_t old_node_index, const uint8_t* block, uint32_t block_size, uint32_t block_index,    
    const uint8_t* sequence, uint8_t seq_len, uint8_t new_weight) {
    GraphNode *old_node = graph_get_node(old_node_index);
    // Validations (unchanged)
    if (!old_node || !block || block_index >= block_size || seq_len == 0) {
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
        return;
    }
    new_saving += old_node->saving_so_far;
            
    uint8_t weight = (new_weight >= SEQ_LENGTH_LIMIT) ? SEQ_LENGTH_LIMIT - 1 : new_weight;
    
    // CREATE NODE FIRST
    GraphNode* new_node = create_new_node(weight, old_node->level+1);
    if (!new_node) {
        fprintf(stderr,"\n node allocation failed level=%d, old_node_id=%d, weight=%u\n", old_node->level+1, old_node->id, weight);
        exit(1);
        return;
    }

    // SET NODE PROPERTIES
    new_node->incoming_weight = weight;
    new_node->saving_so_far = new_saving;
    new_node->compress_sequence = seq_len;
    new_node->level = old_node->level + 1;
    new_node->compress_start_index = seq_start_offset;
    
#ifdef DEBUG
    print_graph_node(new_node, block);
#endif
    // ADD EDGE AFTER NODE IS FULLY INITIALIZED
    if (!graph_add_edge(old_node_index, new_node->id)) {  // Use new_node->id instead of get_current_graph_node_index()
        fprintf(stderr, "Failed to add edge from %u to %u\n", old_node_index, new_node->id);
        return;
    }


}

/**
 * Create root of the graph
 * @block bytes of the block read from the file.
 * @block_size the size of the array block.
*/
static inline void createRoot(const uint8_t* block, uint32_t block_size) {
    /**
     * Return if the block is null or empty.
     * This only happens when we have reached the end of file. a
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
    root->compress_start_index = 0;
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
    #endif
  

}


static void processBlock(const uint8_t *block, uint32_t block_size) {
    if (SEQ_LENGTH_LIMIT <= 1 || block_size == 0 || !block) {
        fprintf(stderr, "Error: Invalid parameters in processBlock\n");
        return;
    }

    // Create the root node
    createRoot(block, block_size);

    for (uint32_t block_index = 1; block_index < block_size; block_index++) {
        uint32_t current_level = get_max_level();

        // Initialize weight cache for this block index
        for (int w = 0; w < SEQ_LENGTH_LIMIT && w <= (int)current_level; w++) {
            graph.weight_cache[w].first_node_with_weight = UINT32_MAX;
            graph.weight_cache[w].weight = 0;
        }
        
        // Process all nodes at current level
        for (uint8_t weight = 0; weight <= SEQ_LENGTH_LIMIT && weight <= (int)current_level; weight++) {
            uint32_t node_count = 0;
            const uint32_t* node_indices = get_nodes_by_weight_and_level(weight, current_level, &node_count);
            
            if (node_count == 0) continue;

            for (uint32_t i = 0; i < node_count; i++) {
                uint32_t node_idx = node_indices[i];
                GraphNode *old_node = graph_get_node(node_idx);
                if (!old_node) {
                    fprintf(stderr, "Error: Null node encountered at index %u\n", node_idx);
                    continue;
                }

                // Check if we've already processed this weight
                if (graph.weight_cache[weight].first_node_with_weight != UINT32_MAX && 
                    graph.weight_cache[weight].first_node_with_weight != node_idx)  {
                    // Reuse children from first node with same weight
                    GraphNode *first_node = graph_get_node(graph.weight_cache[weight].first_node_with_weight);
                    if (!first_node) {
                        fprintf(stderr, "Error: Null first node for weight %u\n", weight);
                        continue;
                    }
                    
                    // Link to existing children
                    for (uint8_t c = 0; c < first_node->child_count; c++) {
                        if (!graph_add_edge(node_idx, first_node->children[c])) {
                            fprintf(stderr, "Failed to add reused edge from %u to %u\n", 
                                    node_idx, first_node->children[c]);
                        }
                    }
                    continue;
                }

                // Only set first_node_with_weight if not already set
                if (graph.weight_cache[weight].first_node_with_weight == UINT32_MAX) {
                    graph.weight_cache[weight].first_node_with_weight = node_idx;
                    graph.weight_cache[weight].weight = weight;
                }

                // Process paths only for first node with this weight
                
                // Uncompressed path (weight increases by 1)
                processNodePath(node_idx, block, block_size, block_index,
                              &block[block_index], 1, old_node->incoming_weight + 1);
                
            }
        }
        // Compressed paths (various sequence lengths)
        for (uint16_t seq_len = 2; seq_len <= current_level+1 && seq_len <= SEQ_LENGTH_LIMIT; seq_len++) {          
            const uint8_t *seq_start = &block[block_index + 1 - seq_len];
            processCompressPath(block, block_size, block_index, seq_start, seq_len, current_level);
        }
    }
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

        // process of block of file at a time.
        processBlock(block, bytesRead);

#ifdef DEBUG
    graphviz_init(&viz, "compression_tree.dot", true);
    // Process entire graph at once
    graphviz_render_full_graph(&viz, block);
    graphviz_finalize(&viz);
#endif
    }

    free(block);
    block = NULL;

    fclose(file);

    return 0;
}
