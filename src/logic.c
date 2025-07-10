#include "logic.h"
#include "graph/graph.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "graph/graph_visualizer.h"
#include "graph/shortest_path.h"
#include "map/sequence_repository_useless.h"
#include "timer.h"

SequenceRepository useless_repo;

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

static GraphNode* root_node = NULL;

uint32_t RLE_nodes_id[MAX_LEVELS]; //every level can have at max one RLE node whose ID is stored here for usefulness.

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
        if (current_level >= MAX_LEVELS) break;

        uint8_t max_sequence = MIN(current_level, MAX_WEIGHTS);

        uint32_t start = block_index - max_sequence + 1;        
        uint8_t repeat_seq_length;
        GraphNode *current_node = NULL;
        // Create RLE node, if any. There could be at most one RLE node per level.
        if (is_RLE_sequence(&repeat_seq_length, max_sequence, start, block)) {
            current_node = create_node(start, max_sequence);
            current_node->is_RLE = 1;
            current_node->repeat_seq_length = repeat_seq_length;
            current_node->is_useless = 0; // mark it useful
            RLE_nodes_id[current_level] = current_node->node_id;
            for (int i = 1; i < max_sequence; i++) {
                uint32_t node_id_rle = RLE_nodes_id[current_level - i];
                if (node_id_rle != 0) { // make previous level nodes useless.
                    get_graph_node(node_id_rle)->is_useless = 1;
                    RLE_nodes_id[current_level - i] = 0; // for speed so that we do not make
                                                         // a same not useless multiple
                                                         // times.
                }
            }
#ifdef DEBUG
        print_graph_node(current_node); // print the newly create node.
#endif

        } else {
            // Make sequences of specific sizes.
            for (uint8_t seq_len = 1; seq_len <= max_sequence; seq_len++) {
                start = block_index - seq_len + 1;
                current_node = create_node(start, seq_len);
                current_node->is_useless = 1; // Assume useless initially
                if (seq_len > 1) {
                    uint32_t node_id = seq_repo_get_node_id(&useless_repo, &block[start], seq_len);
                    if (node_id != UINT32_MAX) { // we have found this sequence before.
                        GraphNode *old_node = get_graph_node(node_id);
                        if (old_node->node_level + seq_len <= current_node->node_level) {
                            old_node->is_useless = 0;     // Mark existing node as useful
                            current_node->is_useless = 0; // Mark current node as useful
                            seq_repo_add(&useless_repo, &block[start], seq_len,
                                         current_node->node_id); // store new id for
                                                                 // future nodes.
                        }
                    } else { // map does not have any record of this
                             // sequence. Add it.
                        seq_repo_add(&useless_repo, &block[start], seq_len, current_node->node_id);
                    }
                }
#ifdef DEBUG
                    print_graph_node(current_node); // print the newly create node.
#endif
            }
        }
    }
    print_all_nodes(block);
    printf("\n*** Done with creating nodes=%u, in %lu ms\n", get_graph_size(), get_elapsed_ms());
    compact_graph(block); // compact the graph by removing useless nodes.

    printf("\n*** Done with nodes compaction, nodes=%u, in %lu ms\n", get_graph_size(), get_elapsed_ms());
#ifdef DEBUG
    visualize_graph(block); // create graph in DOT for visualization.
#endif
    find_shortest_path_to_sink(block); // find shortest path
}
