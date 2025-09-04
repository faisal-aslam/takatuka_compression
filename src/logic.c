#include "logic.h"
#include "best_path_view.h"
#include "compress.h"
#include "decompress.h"
#include "graph.h"
#include "graph_visualizer.h"
#include "seq_freq_map.h"
#include "shortest_path_common.h"
#include "timer.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

double WEIGHT_FREQ = 0.5;
double WEIGHT_LEN  = 0.5;

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

static inline uint8_t RLE_level(const uint8_t *block, uint32_t block_index, uint32_t block_size) {
    uint16_t current_level = get_last_level_index();
    if (is_RLE_sequence(&rle_info.repeat_seq_length, &rle_info.length_of_RLE, MIN(block_size, 255), block_index,
                        block)) {
        // wait for the right level to create node.
        // do not create any RLE nodes before reaching that level.
        // remember data of RLE node to be created later on, at the appropriate level.
        rle_info.next_RLE_level = current_level + rle_info.length_of_RLE - 1;
        rle_info.RLE_offset = block_index;
        return 1;
    }
    return 0;
}
static inline void set_RLE_data() {
    // Create RLE node, if any. There could be at most one RLE node per level.
    uint16_t current_level = get_last_level_index();
    GraphNode *current_node;
    if (current_level == rle_info.next_RLE_level) {
        current_node = create_node(rle_info.RLE_offset, rle_info.length_of_RLE);
        current_node->useless = 0;
        current_node->is_RLE = 1;
        // current_node->repeat_seq_length = rle_info.repeat_seq_length;
        current_node->length_of_RLE = rle_info.length_of_RLE;

#ifdef DEBUG
        //print_graph_node(current_node); // print the RLE node.
#endif
    } else {
        fprintf(stderr, "Illegal set_RLE_data\n");
        abort();
    }
}

static inline void compute_best_path_and_write_in_file_with_alpha_beta(const uint8_t *block) {
    double best_wf = 0.0, best_wl = 0.0;
    long best_size = LONG_MAX;

    // sweep WEIGHT_FREQ from 0.1 to 0.9, WEIGHT_LEN = 1 - WEIGHT_FREQ
    for (int i = 1; i <= 9; i++) {
        WEIGHT_FREQ = i / 10.0;
        WEIGHT_LEN  = 1.0 - WEIGHT_FREQ;

        Path path_state;
        find_best_saving_path(block, &path_state);    
        if (path_state.path_size[PATH_BEST] <= 0) {
            continue; // skip invalid
        }

        final_book_keeping(block, &path_state);
        set_best_path_view(path_state.path_stack[PATH_BEST],
                           path_state.path_freqs[PATH_BEST],
                           path_state.path_size[PATH_BEST]);

        long size_of_compressed_file = write_compressed_output(output_file, block);

        printf("WF=%.2f WL=%.2f -> size=%ld\n", WEIGHT_FREQ, WEIGHT_LEN, size_of_compressed_file);

        if (size_of_compressed_file < best_size) {
            best_size = size_of_compressed_file;
            best_wf = WEIGHT_FREQ;
            best_wl = WEIGHT_LEN;
        }
    }

    printf("\nBest weights: WF=%.2f WL=%.2f -> size=%ld\n", best_wf, best_wl, best_size);

    // rerun with best weights so output_file has final best result
    WEIGHT_FREQ = best_wf;
    WEIGHT_LEN  = best_wl;

    Path path_state;
    find_best_saving_path(block, &path_state);
    if (path_state.path_size[PATH_BEST] > 0) {
        final_book_keeping(block, &path_state);
        set_best_path_view(path_state.path_stack[PATH_BEST],
                           path_state.path_freqs[PATH_BEST],
                           path_state.path_size[PATH_BEST]);
        long size_of_compressed_file = write_compressed_output(output_file, block);
        printf("Written best result to output file, size=%ld\n", size_of_compressed_file);
    } else {
        printf("No valid path found for best weights!\n");
    }
}

static inline long compute_best_path_and_write_in_file(const uint8_t *block) {
    Path path_state;
    find_best_saving_path(block, &path_state);    
    printf("\n%lu: Computed the best possible path \n", get_elapsed_ms());
    if (path_state.path_size[PATH_BEST] <= 0) {
        printf("No path found\n");
        return 0; // no path exist
    }
    final_book_keeping(block, &path_state);
//#ifdef DEBUG
    print_path(0, 1, block, &path_state);
//#endif
    set_best_path_view(path_state.path_stack[PATH_BEST], path_state.path_freqs[PATH_BEST],
                       path_state.path_size[PATH_BEST]);                
    long size_of_compressed_file = write_compressed_output(output_file, block);
    printf("\n%lu: Written the path in output file sized=%ld \n", get_elapsed_ms(), size_of_compressed_file);
    return size_of_compressed_file;
}

long process_block(const uint8_t *block, uint32_t block_size) {

    init_graph();
    init_seq_freq_map();
    create_root();
    printf("%lu: Started processing nodes", get_elapsed_ms());
    rle_info.next_RLE_level = UINT16_MAX;
#ifdef DEBUG
    print_graph_node(get_graph_node(0));
#endif
    for (uint32_t block_index = 0; block_index < block_size; block_index++) {
        GraphNode *current_node = NULL;
        // create new level of the graph
        uint16_t current_level = create_graph_level();

        uint8_t max_sequence = MIN(current_level, MAX_WEIGHTS);
        uint32_t start;
        // special treatment of RLE nodes.
        // special treatment of RLE nodes.
        uint8_t created_rle_node = RLE_level(block, block_index, block_size);
        if (created_rle_node) {
            while (current_level != rle_info.next_RLE_level) {
                current_level = create_graph_level();
                block_index++;
            }
            set_RLE_data();
            continue;
        }
        // Non-RLE nodes: Make sequences of specific sizes.
        for (uint8_t seq_len = 1; seq_len <= max_sequence; seq_len++) {
            if (seq_len > 1 && seq_len < SEQ_LENGTH_START) continue;
            uint8_t parent_count = get_parent_nodes_count_by_level_and_length(current_level, seq_len);
            if (parent_count == 0) {
                continue;
            }
            start = block_index - seq_len + 1;
            current_node = create_node(start, seq_len);

            if (seq_len > 1) {
                uint32_t freq, old_node_id;
                current_node->useless = 1; // by default the node is useless.
                uint32_t index = seq_freq_get_with_index(&block[current_node->offset], seq_len, &freq, &old_node_id);
                if (index != UINT32_MAX) {                             // found, same sequence already in the map.
                    GraphNode *old_node = get_graph_node(old_node_id); // get the old node.                    
                    // as exist multiple times in the graph so mark the old and new node both useful now.
                    current_node->useless = 0;
                    old_node->useless = 0;
                    //However, increment its frequency only when it is not self overlapping.
                    if (old_node->node_level <= get_parent_level(current_node)) {
                        seq_freq_increment_with_index(index, current_node->node_id);
                    }
                } else {
                    seq_freq_increment(&block[current_node->offset], seq_len,
                                       current_node->node_id); // if not in the map then add it.
                }
            }
#ifdef DEBUG
            //print_graph_node(current_node); // print the newly create node.
#endif
        }
    }
    printf("\n%lu: Done creating %u nodes\n", get_elapsed_ms(), graph.size);
    mark_single_freq_nodes_useless(block);
    compact_graph(block);
#ifdef DEBUG
    visualize_graph(block);
#endif
    return compute_best_path_and_write_in_file(block);
    //compute_best_path_and_write_in_file_with_alpha_beta(block);
}
