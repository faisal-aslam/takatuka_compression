#include "logic.h"
#include "graph/graph.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "graph/graph_visualizer.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

static GraphNode* root_node = NULL;

static inline void create_root() {   
    increment_graph_level();
    root_node = get_next_node();
    assert(root_node != NULL);
}

static GraphNode* create_node(uint32_t start, uint8_t length) {
    GraphNode* node = get_next_node();    
    if (!node) return NULL;
    
    node->start_of_sequence = start;
    node->sequence_length = length;
    node->parent_count = 0;
    
    return node;
}

static void link_node_to_parents(GraphNode* node) {
    if (!node || node->sequence_length == 0) return;
    
    uint16_t parent_level = get_total_levels()-node->sequence_length;
    if (parent_level > MAX_LEVELS) {
        fprintf(stderr, "Illegal level \n");
        exit(1);
    }
    
    // Link to all nodes in parent level
    for (uint8_t i = get_level_start_id(parent_level); i < get_level_end_id(parent_level); i++) {
        GraphNode* parent = get_graph_node(i);
        add_link_to_parent(node, parent, 1);
        
    }
}

void process_block(const uint8_t *block, uint32_t block_size) {
    init_graph();
    create_root();
    print_graph_node(get_graph_node(0));

    for (uint32_t block_index = 0; block_index < block_size; block_index++) {
        increment_graph_level();
        uint16_t current_level = get_total_levels();
        if (current_level >= MAX_LEVELS) break;
        
        uint8_t max_sequence = MIN(current_level, MAX_WEIGHTS);
        
        for (uint8_t seq_len = 1; seq_len <= max_sequence; seq_len++) {
            uint32_t start = block_index-seq_len+1;
            GraphNode* node = create_node(start, seq_len);
            
            if (node) {
                link_node_to_parents(node);
            }
            print_graph_node(node);
        }
    }
    visualize_graph(block);
}