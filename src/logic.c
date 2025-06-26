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
    if (get_current_level() == 0) {
        init_graph();
    }
    
    root_node = get_graph_node(0);
    assert(root_node != NULL);
    
    root_node->start_of_sequence = 0;
    root_node->sequence_length = 0;
    root_node->parent_count = 0;
    
    increment_graph_level();
}

static GraphNode* create_node(uint16_t level, uint32_t start, uint8_t length) {
    uint32_t node_id = level * MAX_WEIGHTS + (length-1);
    GraphNode* node = get_graph_node(node_id);
    
    if (!node) return NULL;
    
    node->start_of_sequence = start;
    node->sequence_length = length;
    node->parent_count = 0;
    
    return node;
}

static void link_node_to_parents(GraphNode* node) {
    if (!node || node->sequence_length == 0) return;
    
    uint16_t parent_level = node->start_of_sequence;
    uint32_t parent_base_id = (uint32_t)parent_level * MAX_WEIGHTS;
    
    // Link to all active nodes in parent level
    for (uint8_t i = 0; i < MAX_WEIGHTS; i++) {
        uint32_t parent_id = parent_base_id + i;
        GraphNode* parent = get_graph_node(parent_id);
        
        // Check if parent exists and isn't full
        if (parent && 
            (parent->sequence_length > 0 || parent_level == 0) && 
            node->parent_count < MAX_WEIGHTS) {
            add_link_to_parent(node, parent, 1);
        }
    }
}

void process_block(const uint8_t *block, uint32_t block_size) {
    init_graph();
    create_root();
    
    for (uint32_t block_index = 0; block_index < block_size; block_index++) {
        uint16_t level = block_index + 1;
        if (level >= MAX_LEVELS) break;
        
        uint8_t max_sequence = MIN(level, MAX_WEIGHTS);
        
        for (uint8_t seq_len = 1; seq_len <= max_sequence; seq_len++) {
            uint32_t start = block_index;
            GraphNode* node = create_node(level, start, seq_len);
            
            if (node) {
                link_node_to_parents(node);
            }
        }
        
        increment_graph_level();
    }
    visualize_graph(block);
}