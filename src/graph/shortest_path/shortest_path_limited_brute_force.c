// shortest_path.c

#include "shortest_path_brute_force.h"
#include <math.h>
#include <stdbool.h>

int prune_count = 0;

/**
 * Append PATH_BEST of src into PATH_BEST of dst.
 * Copies node IDs, per-node savings, frequencies, and updates totals.
 */
static inline void append_path_in_reverse(Path *dst, const Path *src) {
    int src_size = src->path_size[PATH_BEST];
    if (src_size < 0) return;

    for (int i = src_size; i >= 0; i--) {
        int dst_idx = ++dst->path_size[PATH_BEST];
        dst->path_stack[PATH_BEST][dst_idx] = src->path_stack[PATH_BEST][i];
        dst->path_per_node_savings[PATH_BEST][dst_idx] = src->path_per_node_savings[PATH_BEST][i];
        dst->path_freqs[PATH_BEST][dst_idx] = src->path_freqs[PATH_BEST][i];
    }

    dst->path_total_saving[PATH_BEST] += src->path_total_saving[PATH_BEST];
    dst->path_total_freq[PATH_BEST] += src->path_total_freq[PATH_BEST];
    dst->path_total_cost[PATH_BEST] += src->path_total_cost[PATH_BEST];
}

/**
 * Correct the reverse path by reversing only the node stack.
 */
static inline void correct_the_reverse_path(Path *final_reverse_path) {
    int size = final_reverse_path->path_size[PATH_BEST];
    if (size < 0) return;

    int left = 0;
    int right = size;

    while (left < right) {
        int tmp = final_reverse_path->path_stack[PATH_BEST][left];
        final_reverse_path->path_stack[PATH_BEST][left] = final_reverse_path->path_stack[PATH_BEST][right];
        final_reverse_path->path_stack[PATH_BEST][right] = tmp;

        left++;
        right--;
    }
}

uint8_t is_level_empty(uint16_t level) {
    if (get_level_start_id(level) == get_level_end_id(level) ||
        get_level_start_id(level) + 1 == get_level_end_id(level))
        return 1;
    else
        return 0;
}
/*
We start from the root. Then we find shortest path from root's level (0) plus max_brute_force_levels.
Then we continue going down the root.
In order to find better path we use seq_freq map with high frequence sequences as the seed.
*/
void find_limited_bute_force_path(const uint8_t *block, uint32_t max_brute_force_levels, Path *path_state) {

    uint32_t dest_node_id = 0; // at the start our destination (sink) is root but it will change later as we proceed.

    uint16_t level = MIN(graph.total_levels, max_brute_force_levels); // level to end at (source level).

    // Seed for finding path. If we choose less frequent sequences then path with only one encounter of such sequences
    // will grow resulting in bad path cost. Thus, must choose frequent sequences.
    // keep only sequences that appear greater than 4 times.
    seq_freq_filter_freqs_and_length(2, 7, 2);
    //init_seq_freq_map();

    path_init(path_state); // must initialize the path before populating it correctly.
    visualize_graph(block);
#ifdef DEBUG
    seq_freq_map_print(); // to verify the map contents.
#endif

    // continue till we are at the last level (a.k.a source level)
    while (1) {
        Path intermediate_path;

        //check if the level is empty then try again.
        if (is_level_empty(level) && get_last_level_index() > level) {
            level ++;
            continue; //try again.
        }
        // the following finds path burte-forcely.
        find_best_saving_path_to_a_node(block, level, dest_node_id, &intermediate_path);

#ifdef DEBUG
        print_path(0, 1, block, &intermediate_path); // to verify the intermediate path.
        seq_freq_map_print();                        // check if the map is correct.
#endif
        if (intermediate_path.path_size[PATH_BEST] < 0) {
            fprintf(stderr, "Unable to find a valid path. Level=%u, destination_node=%u\n", level, dest_node_id);
            abort();
        }
        // after computing the path append with the previously generated path.
        append_path_in_reverse(path_state, &intermediate_path);

#ifdef DEBUG
        print_path(0, 1, block, path_state); // check if append was successful.
#endif
        if (level == get_last_level_index()) {
            break; // we are done.
        }
        // now preparing for the next iteration.
        dest_node_id = intermediate_path.path_stack[PATH_BEST][0]; // find the new destination.
        level = MIN(get_last_level_index(),
                    get_graph_node(dest_node_id)->node_level + max_brute_force_levels); // new level.
    }
    correct_the_reverse_path(path_state);
}

void find_best_saving_path(const uint8_t *block, uint16_t starting_level, Path *path_state) {
    (void)starting_level; // not used but kept for consistency.

    find_limited_bute_force_path(block, 51, path_state);
}