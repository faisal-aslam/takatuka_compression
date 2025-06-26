#include "logic.h"

/*
Graph Node Construction:

- Root Node:
  Create a special root node at level 0. This node does not represent any data, 
  so its sequence_length is set to 0. It serves as the starting point for all paths.

- For each subsequent level (which corresponds to block_index + 1), 
  create up to MAX(level, SEQ_LENGTH_LIMIT) nodes.

  Each node at a given level represents a sequence of data with:
    - sequence_length ranging from 1 up to MAX(level, SEQ_LENGTH_LIMIT)
    - start_of_sequence set to block_index = level - 1

  Example:
  -- Level 1 (block_index = 0):
     - Create one node with sequence_length = 1 and start_of_sequence = 0.
     - Link this node to the root.

  -- Level 2 (block_index = 1):
     - Create up to MAX(2, SEQ_LENGTH_LIMIT) nodes:
         - First node: sequence_length = 1, start_of_sequence = 1
         - Second node: sequence_length = 2, start_of_sequence = 1

  -- Level i (block_index = i - 1):
     - Create up to MAX(i, SEQ_LENGTH_LIMIT) nodes:
         - First node: sequence_length = 1, start_of_sequence = i - 1
         - Last node:  sequence_length = MAX(i, SEQ_LENGTH_LIMIT), start_of_sequence = i - 1

  How to create Links between nodes:
  -- While creating nodes we will also be creating links between nodes. 
  * Root node has no outgoing link (as it does not correspond to any  data i.e. its sequence_length=start_of_sequence=0)
  * For all other nodes the links are created based on node's data (start_of_sequence)
  That is any node with start_of_sequence=j, will point to all the nodes of level j-1.

  
*/


#include "logic.h"
#include "graph/graph.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h> // For min/max macros

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

// Static root node pointer for easy access
static GraphNode* root_node = NULL;

static inline void create_root() {
    // Initialize the graph if not already done
    if (get_current_level() == 0) {
        init_graph();
    }
    
    // Get root node 
    root_node = get_graph_node(0);
    assert(root_node != NULL);
    
    // Initialize root node properties
    root_node->start_of_sequence = 0;
    root_node->sequence_length = 0;
    root_node->parent_count = 0;  // Root has no parents
    
    // Set current level to 1 (root is level 0)
    increment_graph_level();
}

static GraphNode* create_node(uint16_t level, uint32_t start, uint8_t length) {
    uint32_t node_id = level * MAX_WEIGHTS + (length-1);
    GraphNode* node = get_graph_node(node_id);
    
    if (!node) return NULL;
    
    // Initialize node properties
    node->start_of_sequence = start;
    node->sequence_length = length;
    node->parent_count = 0;
    
    return node;
}

static void link_node_to_parents(GraphNode* node) {
    if (!node || node->sequence_length == 0) return; // Skip root
    
    uint16_t parent_level = node->start_of_sequence;
    uint16_t max_parents = MIN(MAX_WEIGHTS, parent_level + 1);
    
    // Link to all nodes in parent level
    GraphNode* parent_level_nodes = get_all_nodes_of_level(parent_level);
    if (!parent_level_nodes) return;
    
    for (uint8_t i = 0; i < max_parents; i++) {
        GraphNode* parent = &parent_level_nodes[i];
        if (parent->sequence_length > 0 || parent_level == 0) { // Include root
            add_link_to_parent(node, parent, 1); // Defaultcost of 1
        }
    }
}

void process_block(const uint8_t *block, uint32_t block_size) {
    //init_graph only once per block.
    init_graph();
    // Create root node (level 0)
    create_root();
    
    // Process each block (level = block_index + 1)
    for (uint32_t block_index = 0; block_index < block_size; block_index++) {
        uint16_t level = block_index + 1;
        if (level >= MAX_LEVELS) break;
        
        uint8_t max_sequence = MIN(level, MAX_WEIGHTS);
        
        // Create nodes for this level
        for (uint8_t seq_len = 1; seq_len <= max_sequence; seq_len++) {
            uint32_t start = block_index;
            GraphNode* node = create_node(level, start, seq_len);
            
            // Create links to parent nodes
            if (node) {
                link_node_to_parents(node);
            }
        }
        
        // Update current level
        increment_graph_level();
    }
}