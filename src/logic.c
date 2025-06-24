// logic.c

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
#include "graph/graph_visualizer.h"
#include "logic.h"
#include "map/node_map_pool.h"



uint16_t total_codes = 0;

static void process_uncompressed_path(uint8_t weight, const uint8_t* block, uint32_t block_size, uint32_t block_index, const uint8_t* sequence, uint32_t current_level);

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

static inline void process_compressed_path(const uint8_t* block, uint32_t block_size,
                                           uint32_t block_index, uint8_t seq_len,
                                           uint32_t current_level) {
    // Calculate the starting index of the compressed sequence in the block
    uint32_t seq_start_offset = block_index + 1 - seq_len;

    // Validate that the sequence does not exceed block bounds
    if (seq_start_offset >= block_size || (seq_start_offset + seq_len) > block_size) {
        fprintf(stderr, "\n[Error] Invalid sequence start: index out of bounds\n");
        return;
    }

    // Create a new graph node at the next level (current_level + 1)
    GraphNode* new_node = create_new_node(0, current_level + 1);
    if (!new_node) {
        fprintf(stderr, "\n[Error] Node allocation failed at level=%u, weight=0\n", current_level + 1);
        exit(1);
    }

    // Assign compressed sequence metadata to the new node
    new_node->compress_sequence_length = seq_len;
    new_node->compress_start_index = seq_start_offset;

#ifdef DEBUG
    print_graph_node(new_node, block);
#endif

    // Loop over all parent weights that could produce this compressed node
    for (uint8_t parent_weight = seq_len - 1;
         parent_weight < SEQ_LENGTH_LIMIT && parent_weight <= (uint8_t)current_level;
         parent_weight++) {
        
        uint32_t count = 0;
        const uint32_t* indexes = get_nodes_by_weight_and_level(parent_weight, current_level, &count);
        if (count == 0) continue;

        /**
         * Step 1: Skip (seq_len - 2) parent hops upward from the first parent
         * Step 2: Copy all parent links from that ancestor to the new node
         */

        uint32_t skip_node_id = indexes[0];
        GraphNode* ancestor = graph_get_node(skip_node_id);

        for (int i = 0; i < seq_len - 2; i++) {
            // Ensure there's exactly one parent to follow during upward skip
            if (ancestor->parent_link_count != 1) {
                fprintf(stderr, "[Error] Expected exactly one parent during skip (node ID = %u)\n", ancestor->id);
                exit(1);
            }

            // Move one level up
            ParentLink p_link = ancestor->parent_links[0];
            ancestor = graph_get_node(p_link.parent_node_id);
        }

        // Add all parents of the ancestor as parents of the new node
        for (int i = 0; i < ancestor->parent_link_count; i++) {
            GraphNode* final_parent = graph_get_node(ancestor->parent_links[i].parent_node_id);
            if (!graph_add_parent_edge(new_node, final_parent, block)) {
                fprintf(stderr, "[Error] Failed to add edge from node %u to parent %u\n",
                        new_node->id, final_parent->id);
                return;
            }
        }
    }
}


/**
 * Creates a new node with the given sequence (typically uncompressed) at the next level
 * and connects it to all parent nodes of a given weight at the current level.
 *
 * Ensures:
 * - Node is created only once per call.
 * - All eligible parent nodes are connected via parent edges.
 *
 * @param weight          Weight of parent nodes to search for.
 * @param block           The input block data.
 * @param block_size      Size of the input block.
 * @param block_index     Current index in the block being processed.
 * @param sequence        The sequence (typically of length 1) for the new node.
 * @param current_level   The level in the graph to look for parent nodes.
 */
static inline void process_uncompressed_path(uint8_t weight, const uint8_t *block,
                            uint32_t block_size, uint32_t block_index,
                            const uint8_t *sequence, uint32_t current_level) {
    // Step 1: Get all nodes with the given weight at the current level
    uint32_t node_count = 0;
    const uint32_t *node_indices = get_nodes_by_weight_and_level(weight, current_level, &node_count);
    if (node_count == 0) return;  // No eligible parents to process

    // Step 2: Validate parameters
    if (!block || block_index >= block_size) {
        fprintf(stderr, "\nprocess_uncompressed_path: invalid input block or index\n");
        return;
    }

    // Step 3: Determine start of the sequence (usually just one symbol)
    uint32_t seq_start_offset = block_index;
    if (seq_start_offset + 1 > block_size) {
        fprintf(stderr, "\nprocess_uncompressed_path: sequence goes beyond block\n");
        return;
    }

    // Step 4: Calculate potential savings from this sequence
    int32_t new_saving = calculate_savings(sequence, 1, NULL);
    if (new_saving == INT_MIN) {
        return;  // Skip node creation if not beneficial
    }

    // Step 5: Use first parent node to determine new weight and level
    GraphNode *first_parent = graph_get_node(node_indices[0]);
    if (!first_parent) {
        fprintf(stderr, "\nprocess_uncompressed_path: first parent node is null\n");
        return;
    }

    uint8_t new_weight = first_parent->incoming_weight + 1;
    if (new_weight >= SEQ_LENGTH_LIMIT) {
        new_weight = SEQ_LENGTH_LIMIT - 1;
    }

    // Step 6: Create the new node once
    GraphNode *new_node = create_new_node(new_weight, first_parent->level + 1);
    if (!new_node) {
        fprintf(stderr, "\nFailed to allocate new node at level %d (weight %u)\n",
                first_parent->level + 1, new_weight);
        exit(EXIT_FAILURE);
    }

    // Set node metadata
    new_node->compress_sequence_length = 1;              // Only one symbol
    new_node->compress_start_index = seq_start_offset;

#ifdef DEBUG
    print_graph_node(new_node, block);
#endif

    // Step 7: Add all parent edges from nodes of same weight at this level
    for (uint32_t i = 0; i < node_count; i++) {
        uint32_t parent_id = node_indices[i];
        GraphNode *parent = graph_get_node(parent_id);
        if (!parent) {
            fprintf(stderr, "Warning: Null parent node at index %u\n", parent_id);
            continue;
        }

        if (!graph_add_parent_edge(new_node, parent, block)) {
            fprintf(stderr, "Failed to add edge from new node %u to parent %u\n",
                    new_node->id, parent_id);
        }
    }
}


/**
 * Create root of the graph
 * @block bytes of the block read from the file.
 * @block_size the size of the array block.
*/
static inline void create_root(const uint8_t* block, uint32_t block_size) {
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

    //empty node is the real root but does not contain any data. It is useful
    //for the other nodes to point.
    GraphNode* empty = create_new_node(0, 0);
    // create root node and set its values 
    // root's weight=1 and level=1
    GraphNode* root = create_new_node(1, 1);   
    
    root->compress_sequence_length = 1; //there is nothing to compress yet at the root level.
    root->compress_start_index = 0; //start of the block.
    graph_add_parent_edge(root, empty, block);
    #ifdef DEBUG
    printf("\nCreated new root and empty nodes :\n");
    print_graph_node(empty, block);
    print_graph_node(root, block);
    #endif
  

}

void process_block(const uint8_t *block, uint32_t block_size) {
    if (SEQ_LENGTH_LIMIT <= 1 || block_size == 0 || !block) {
        fprintf(stderr, "Error: Invalid parameters in process_block\n");
        return;
    }

    // Create the root node
    create_root(block, block_size);

    /**
     * Create a new level of the graph corresponding to each byte of the block. 
     */
    for (uint32_t block_index = 1; block_index < block_size; block_index++) {

        // Only print when percentage changes (avoid duplicate messages)
        if (block_index % 10 == 0) {
            // Calculate current progress percentage
            int current_percent = (int)((double)block_index / block_size * 100);            
            printf("\rProcessing block: %3d%% complete", current_percent);
            fflush(stdout); // Ensure immediate output
            
        }
        uint32_t current_level = get_max_level();

        // Compressed paths (various sequence lengths)
        for (uint16_t seq_len = 2;
             seq_len <= current_level + 1 && seq_len <= SEQ_LENGTH_LIMIT;
             seq_len++) {            
            process_compressed_path(block, block_size, block_index, seq_len, current_level);
        }


        // Process all nodes at current level
        uint8_t upper = current_level < SEQ_LENGTH_LIMIT
                  ? (uint8_t)current_level
                  : SEQ_LENGTH_LIMIT;
        for (uint8_t weight = 0; weight <= upper; weight++) {          
            // Process paths only for first node with this weight
            process_uncompressed_path(weight, block, block_size, block_index,
                                &block[block_index], current_level);
            
        }
    }
    printf("\rProcessing block: %3d%% complete", 100);
    fflush(stdout); // Ensure immediate output

#ifdef DEBUG
    GraphVisualizer viz;
    graphviz_init(&viz, "compression_tree.dot", true);
    // Process entire graph at once
    graphviz_render_full_graph(&viz, block);
    graphviz_finalize(&viz);
#endif
    //find and print the best path.
    find_and_print_best_path(block);

}

