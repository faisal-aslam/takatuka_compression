#include "logic.h"
#include "graph/graph.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "graph/graph_visualizer.h"
#include "graph/shortest_path.h"
#include "map/sequence_repository_useless.h"


#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

static GraphNode* root_node = NULL;

static inline void create_root() {   
    increment_graph_level();
    root_node = get_next_node();
    assert(root_node != NULL);
}

void process_block(const uint8_t *block, uint32_t block_size) {
    init_graph();
    create_root();

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
            GraphNode* node = get_next_node();    
            if (node) {    
                node->offset = start;
                node->sequence_length = seq_len;
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
