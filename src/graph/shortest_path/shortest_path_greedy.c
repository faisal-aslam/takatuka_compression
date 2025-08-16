// shortest_path_greedy.c

#include "shortest_path_common.h"
#include <float.h>
#include <math.h>
#include <stdbool.h>


void find_best_saving_path(const uint8_t *block, uint16_t starting_level, Path* path_state) {
    path_init(path_state);
    uint32_t max_saving_node_ids[MAX_LEVELS];
    compute_max_saving_node_ids(block, max_saving_node_ids);

    while (1) {
        uint32_t node_id = max_saving_node_ids[starting_level];
        if (node_id == UINT32_MAX) {
            fprintf(stderr, "No node found for level %u\n", starting_level);
            abort();
        }

        GraphNode *node = get_graph_node(node_id);
        uint32_t freq=1, dummy_id;
        if (node->sequence_length > 1 && !node->is_RLE) {
           seq_freq_get(&block[node->offset], node->sequence_length, &freq, &dummy_id);
        }
        double node_savings = calc_savings(node, freq);

        path_state->path_total_saving[PATH_BEST] += node_savings;
        path_state->path_size[PATH_BEST]++;
        path_state->path_total_freq[PATH_BEST] += freq;
        path_state->path_per_node_savings[PATH_BEST][path_state->path_size[PATH_BEST]] = node_savings;
        path_state->path_stack[PATH_BEST][path_state->path_size[PATH_BEST]] = node_id;

        if (node->node_id == 0) break;
        starting_level = get_parent_level(node);
    }
}

