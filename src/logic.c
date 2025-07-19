#include "logic.h"
#include "compress.h"
#include "decompress.h"
#include "graph/graph.h"
#include "graph/graph_visualizer.h"
#include "graph/shortest_path.h"
#include "sequence_repository_useless.h"
#include "timer.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static GraphNode *root_node = NULL;

static inline void create_root() {
    create_graph_level();
    root_node = get_next_node();
    assert(root_node != NULL);
}

static inline GraphNode *create_node(uint32_t start, uint8_t length) {
    GraphNode *node = get_next_node();
    if (!node) return NULL;

    node->offset = start;
    node->sequence_length = length;
    return node;
}

void process_block(const uint8_t *block, uint32_t block_size) {

    init_graph();
    create_root();

#ifdef DEBUG
    print_graph_node(get_graph_node(0));
#endif
    for (uint32_t block_index = 0; block_index < block_size; block_index++) {
        GraphNode *current_node = NULL;
        create_graph_level(); // create new level of the graph
        uint16_t current_level = get_last_level_index();
        if (current_level >= MAX_LEVELS) {
            fprintf(stderr, "Number of levels are more than allowed\n");
            abort();
        }
        uint8_t max_sequence = MIN(current_level, MAX_WEIGHTS);
        uint8_t start;
        // Make sequences of specific sizes.
        for (uint8_t seq_len = 1; seq_len <= max_sequence; seq_len++) {
            start = block_index - seq_len + 1;
            current_node = create_node(start, seq_len);
#ifdef DEBUG
            print_graph_node(current_node); // print the newly create node.
#endif
        }

#ifdef DEBUG
        visualize_graph(block); // create graph in DOT for visualization.
#endif
    }
}
