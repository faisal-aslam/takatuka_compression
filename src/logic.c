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
    //seq_repo_init(&useless_repo, INITIAL_CAPACITY);

    uint8_t levels_to_keep[block_size + 1];
    memset(levels_to_keep, 0, (block_size + 1) * sizeof(uint8_t));
    levels_to_keep[0] = 1;

#ifdef DEBUG
    print_graph_node(get_graph_node(0));
#endif
    uint16_t next_RLE_level = 0;
    uint8_t repeat_seq_length = 0, length_of_RLE = 0;
    uint32_t RLE_offset = 0;
    for (uint32_t block_index = 0; block_index < block_size; block_index++) {

        GraphNode *current_node = NULL;
        // Create RLE node, if any. There could be at most one RLE node per level.
        if (next_RLE_level < get_last_level_index() &&
            is_RLE_sequence(&repeat_seq_length, &length_of_RLE, MIN(block_size, 255), block_index, block)) {
            // wait for the right level to create node.
            next_RLE_level = get_last_level_index() + length_of_RLE;
            RLE_offset = block_index;
            // do not create any RLE nodes before reaching that level.
            // remember data of RLE node to be created later on on the appropriate level.
        }
        create_graph_level(); // create new level of the graph
        uint16_t current_level = get_last_level_index();
        if (current_level >= MAX_LEVELS) break;
        uint8_t max_sequence = MIN(current_level, MAX_WEIGHTS);
        uint8_t start;
        // Make sequences of specific sizes.
        for (uint8_t seq_len = 1; seq_len <= max_sequence; seq_len++) {            
            start = block_index - seq_len + 1;
            current_node = create_node(start, seq_len);
            if (seq_len > 1) {
                uint32_t node_id = 0;//seq_repo_get_node_id(&useless_repo, &block[start], seq_len);                
            }
#ifdef DEBUG
            print_graph_node(current_node); // print the newly create node.
#endif
        }
        if (current_level == next_RLE_level) {
            current_node = create_node(RLE_offset, length_of_RLE);
            current_node->is_RLE = 1;
            current_node->repeat_seq_length = repeat_seq_length;            
            current_node->length_of_RLE = length_of_RLE;
#ifdef DEBUG
            print_graph_node(current_node); // print the RLE node.
#endif
        }
    }
    /*levels_to_keep[get_last_level_index()] = 1; // keep the last level level.
    printf("\n*** Done with creating nodes=%u, in %lu ms\n", get_graph_size(), get_elapsed_ms());
    compact_graph(block, levels_to_keep); // compact the graph by removing useless nodes.

    printf("\n*** Done with nodes compaction, nodes=%u, in %lu ms\n", get_graph_size(), get_elapsed_ms());
#ifdef DEBUG
    visualize_graph(block); // create graph in DOT for visualization.
#endif
    find_shortest_path_to_sink(block); // find shortest path
    // finally write compress file.
    write_compressed_output("output.fa", block);
    read_compressed_file("output.fa", block);*/
}
