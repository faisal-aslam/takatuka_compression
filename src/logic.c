#include "logic.h"
#include "graph/graph.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "graph/graph_visualizer.h"
#include "graph/shortest_path.h"
#include "map/sequence_repository_useless.h"

SequenceRepository useless_repo;

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
    
    node->offset = start;
    node->sequence_length = length;
    return node;
}

void process_block(const uint8_t *block, uint32_t block_size) {
    init_graph();
    create_root();
    seq_repo_init(&useless_repo, INITIAL_CAPACITY);

#ifdef DEBUG
    print_graph_node(get_graph_node(0));
#endif

    for (uint32_t block_index = 0; block_index < block_size; block_index++) {
        increment_graph_level();
        uint16_t current_level = get_last_level_index();
        if (current_level >= MAX_LEVELS)
            break;

        uint8_t max_sequence = MIN(current_level, MAX_WEIGHTS);

        // Make sequences of specific sizes. 
        for (uint8_t seq_len = 1; seq_len <=  max_sequence; seq_len++) {
            uint32_t start = block_index - seq_len + 1;
            GraphNode *node = create_node(start, seq_len);

            if (seq_len > 1) {
                uint32_t node_id = seq_repo_get_node_id(&useless_repo, &block[start], seq_len);
                node->isUseless = 1; // Assume useless initially

                if (node_id != UINT32_MAX) {
                    GraphNode *g_node = get_graph_node(node_id);
                    if (g_node) g_node->isUseless = 0; // Mark existing node as useful
                    node->isUseless = 0;                // Mark current node as useful
                } else {
                    seq_repo_add(&useless_repo, &block[start], seq_len, node->node_id);
                    // Still marked useless until repeated
                }
            } else {
                // 1-length sequences are always useful and not tracked
                node->isUseless = 0;
            }

#ifdef DEBUG
            print_graph_node(node);
#endif
        }
    }
    compact_graph(block); //compact the graph by removing useless nodes.
#ifdef DEBUG
    visualize_graph(block); //create graph in DOT for visualization.
#endif    
    find_shortest_path_to_sink(block); //find shortest path    

}
