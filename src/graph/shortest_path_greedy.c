// shortest_path_greedy.c

#include "shortest_path_common.h"
#include <float.h>
#include <math.h>
#include <stdbool.h>

Path path_state;

void compute_max_saving_node_ids(const uint8_t *block, uint32_t *max_ids) {
    for (uint16_t level = 0; level < graph.total_levels; level++) {
        uint32_t start = graph.first_node_of_level[level];
        uint32_t end = (level + 1 < graph.total_levels) ? graph.first_node_of_level[level + 1] : graph.size;

        if (start == UINT32_MAX || start >= end) {
            max_ids[level] = UINT32_MAX; // No nodes in this level
            continue;
        }

        double max_saving = -1.0;
        uint32_t best_node_id = UINT32_MAX;

        for (uint32_t i = start; i < end; i++) {
            GraphNode *node = &graph.nodes[i];
            if (node->useless) continue;

            uint32_t freq = 0;
            if (node->sequence_length > 1 && !node->is_RLE) {
                uint32_t dummy_id;
                if (!seq_freq_get(&block[node->offset], node->sequence_length, &freq, &dummy_id))
                    continue; // skip if not found
            } else {
                freq = 1; // use dummy frequency for RLE and short sequences
            }

            double saving = calc_savings(node, freq);
            if (saving > max_saving) {
                max_saving = saving;
                best_node_id = node->node_id;
            }
        }

        max_ids[level] = best_node_id;
    }
}


/*void find_best_saving_path(const uint8_t *block, uint16_t starting_level) {
    path_init();
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

        path_state.path_total_saving[PATH_BEST] += node_savings;
        path_state.path_size[PATH_BEST]++;
        path_state.path_total_freq[PATH_BEST] += freq;
        path_state.path_per_node_savings[PATH_BEST][path_state.path_size[PATH_BEST]] = node_savings;
        path_state.path_stack[PATH_BEST][path_state.path_size[PATH_BEST]] = node_id;

        if (node->sequence_length > 1 && !node->is_RLE) {
            uint32_t index = seq_freq_get_with_index(&block[node->offset], node->sequence_length, &freq, &node_id);
            seq_freq_set_existing(index, freq + 3, node_id);
        }

        if (node->node_id == 0) break;
        starting_level = get_parent_level(node);
    }
}*/

static inline void get_max_saving_node_of_level(uint16_t level, const uint8_t *block, double *node_savings,
                                                uint32_t *node_freq, uint32_t *max_savings_node_id) {

    uint32_t start_id = get_level_start_id(level);
    uint32_t end_id = get_level_end_id(level);
    *node_savings = -1;
    *max_savings_node_id = UINT32_MAX;
    for (uint32_t id = start_id; id < end_id; id++) {
        GraphNode *node = get_graph_node(id);        
        if (node->useless) continue;
        if (node->node_level != level) {
            fprintf(stderr, "Illegal level \n");
            abort();
        }
        uint32_t node_id;
        seq_freq_get(&block[node->offset], node->sequence_length, node_freq, &node_id);
        double savings = calc_savings(node, *node_freq);
        if (savings > *node_savings) {
            *max_savings_node_id = node->node_id;
            *node_savings = savings;
        }
    }
    if (*max_savings_node_id == UINT32_MAX) {
        fprintf(stderr, "Illegal level %u \n", level);
        abort();
    }
}

void find_best_saving_path(const uint8_t *block, uint16_t starting_level) {

    path_init(); // Reset path state
    while (1) {
        double node_savings;
        uint32_t freq, node_id;
        get_max_saving_node_of_level(starting_level, block, &node_savings, &freq, &node_id);
        GraphNode *node = get_graph_node(node_id);
        path_state.path_total_saving[PATH_BEST] += node_savings;
        path_state.path_size[PATH_BEST]++;
        path_state.path_total_freq[PATH_BEST] += freq;
        path_state.path_per_node_savings[PATH_BEST][path_state.path_size[PATH_BEST]] = node_savings;
        path_state.path_stack[PATH_BEST][path_state.path_size[PATH_BEST]] = node_id;
        if (node->sequence_length > 1 &&
            !node->is_RLE) { // so that other nodes also select this node with higher probability.
            uint32_t index = seq_freq_get_with_index(&block[node->offset], node->sequence_length, &freq, &node_id);
            seq_freq_set_existing(index, freq + 3, node_id);
        }
        if (node->node_id == 0) break;
        starting_level = get_parent_level(node);
    }
}



