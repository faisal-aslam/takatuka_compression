#include "logic.h"
#include "compress.h"
#include "decompress.h"
#include "graph.h"
#include "graph_visualizer.h"
#include "seq_freq_map.h"
#include "shortest_path.h"
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
// use to store
typedef struct {
    uint32_t RLE_offset;       // The starting point of block of RLE
    uint16_t next_RLE_level;   // we must wait till the next RLE level to create the node.
    uint8_t repeat_seq_length; // number of bytes repeated. Like for ABCABCABCABC repeat_seq_length=3
    uint8_t length_of_RLE;     // length of RLE sequence. Like for ABCABCABCABC length_of_RLE=4
} RLE_info;

RLE_info rle_info;

static inline GraphNode *create_node(uint32_t start, uint8_t length) {
    GraphNode *node = get_next_node();
    if (!node) return NULL;

    node->offset = start;
    node->sequence_length = length;
    return node;
}

static inline uint8_t RLE_logic(const uint8_t *block, uint32_t block_index, uint32_t block_size) {
    // Create RLE node, if any. There could be at most one RLE node per level.
    uint16_t current_level = get_last_level_index();
    if (rle_info.next_RLE_level < current_level && is_RLE_sequence(&rle_info.repeat_seq_length, &rle_info.length_of_RLE,
                                                                   MIN(block_size, 255), block_index, block)) {
        // wait for the right level to create node.
        // do not create any RLE nodes before reaching that level.
        // remember data of RLE node to be created later on, at the appropriate level.
        rle_info.next_RLE_level = current_level + rle_info.length_of_RLE - 1;
        rle_info.RLE_offset = block_index;
        return;
    }
    GraphNode *current_node;
    if (current_level == rle_info.next_RLE_level) {
        current_node = create_node(rle_info.RLE_offset, rle_info.length_of_RLE);
        current_node->is_RLE = 1;
        current_node->repeat_seq_length = rle_info.repeat_seq_length;
        current_node->length_of_RLE = rle_info.length_of_RLE;
#ifdef DEBUG
        print_graph_node(current_node); // print the RLE node.
#endif
        return 1;
    }
    return 0;
}

void process_block(const uint8_t *block, uint32_t block_size) {

    init_graph();
    init_seq_freq_map();
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
        uint8_t created_rle_node = RLE_logic(block, block_index, block_size);
        if (created_rle_node){
            max_sequence = 1;
        }
        // Make sequences of specific sizes.
        for (uint8_t seq_len = 1; seq_len <= max_sequence; seq_len++) {
            start = block_index - seq_len + 1;
            current_node = create_node(start, seq_len);
            if (seq_len > 1) {
                seq_freq_increment(&block[current_node->offset], seq_len);
            }
#ifdef DEBUG
            print_graph_node(current_node); // print the newly create node.
#endif
        }
        
    }
    
#ifdef DEBUG
    visualize_graph(block); // create graph in DOT for visualization.
#endif
    find_shortest_path_to_sink(block);
}
