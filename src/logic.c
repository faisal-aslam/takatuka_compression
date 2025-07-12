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

static inline void create_root() {   
    create_graph_level();
    root_node = get_next_node();
    assert(root_node != NULL);
}

static inline GraphNode* create_node(uint32_t start, uint8_t length) {
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

    uint8_t levels_to_keep[block_size+1];
    memset(levels_to_keep, 0, (block_size+1)*sizeof(uint8_t));
    levels_to_keep[0]=1;

#ifdef DEBUG
    print_graph_node(get_graph_node(0));
#endif

    for (uint32_t block_index = 0; block_index < block_size; block_index++) {

        uint8_t repeat_seq_length=0, length_of_RLE=0;
        GraphNode *current_node = NULL;
        // Create RLE node, if any. There could be at most one RLE node per level.
        if (is_RLE_sequence(&repeat_seq_length, &length_of_RLE, MIN(block_size, SEQ_LENGTH_LIMIT), block_index,
                            block)) {            
            mass_increment_levels(length_of_RLE-1);
            create_graph_level();
            current_node = create_node(block_index, length_of_RLE);
            current_node->is_RLE = 1;
            current_node->repeat_seq_length = repeat_seq_length;
            current_node->is_useless = 0; // mark it useful
            block_index = block_index + length_of_RLE - 1; // compensating for increase of block index at the end of for loop.
            
#ifdef DEBUG
            print_graph_node(current_node); // print the newly create node.
#endif

        } else {
            create_graph_level(); //create new level of the graph
            uint16_t current_level = get_last_level_index();
            if (current_level >= MAX_LEVELS) break;
            uint8_t max_sequence = MIN(current_level, MAX_WEIGHTS);
            uint8_t start;
            // Make sequences of specific sizes.
            for (uint8_t seq_len = 1; seq_len <= max_sequence; seq_len++) {
                if (graph.first_node_of_level[current_level-seq_len] == UINT32_MAX) continue; 
                start = block_index - seq_len + 1;                
                current_node = create_node(start, seq_len);
                current_node->is_useless = 1; // Assume useless initially
                if (seq_len > 1) {
                    uint32_t node_id = seq_repo_get_node_id(&useless_repo, &block[start], seq_len);
                    if (node_id != UINT32_MAX) { // we have found this sequence before.
                        GraphNode *old_node = get_graph_node(node_id);
                        if (old_node->node_level + seq_len == current_node->node_level) {
                            old_node->is_useless = 0;     // Mark existing node as useful
                            current_node->is_useless = 0; // Mark current node as useful
                            seq_repo_add(&useless_repo, &block[start], seq_len,
                                         current_node->node_id); // store new id for
                                                                 // future nodes.
                            uint32_t current_node_id = current_node->node_id;
                            for (uint8_t j = seq_len - 1; j >= 1; j--) {
                                if (current_node_id - j < graph.size) {
                                    GraphNode *previous_node_cl = get_graph_node(current_node_id - j);
                                    if (previous_node_cl->node_level == current_level) previous_node_cl->is_useless = 1;
                                }
                                if (old_node->node_id - j < graph.size) {
                                    GraphNode *previous_node_ol = get_graph_node(old_node->node_id - j);
                                    if (previous_node_ol->node_level == old_node->node_level)
                                        previous_node_ol->is_useless = 1;
                                }
                            }
                        } else if (old_node->node_level+seq_len < current_node->node_level) {
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
    levels_to_keep[get_last_level_index()] = 1; //keep the last level level.
    printf("\n*** Done with creating nodes=%u, in %lu ms\n", get_graph_size(), get_elapsed_ms());
    compact_graph(block, levels_to_keep); // compact the graph by removing useless nodes.

    printf("\n*** Done with nodes compaction, nodes=%u, in %lu ms\n", get_graph_size(), get_elapsed_ms());
#ifdef DEBUG
    visualize_graph(block); // create graph in DOT for visualization.
#endif
    find_shortest_path_to_sink(block); // find shortest path
}
