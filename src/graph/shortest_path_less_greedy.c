// shortest_path_less_greedy.c

#include "shortest_path_common.h"
#include <float.h>
#include <math.h>
#include <stdbool.h>

Path path_state;

static void compact_graph_with_savings(const uint8_t *block, uint32_t *max_saving_node_ids) {
    if (graph.size == 0) return;

    uint32_t write_idx = 0;
    uint32_t current_level = 0;

    for (uint32_t l = 0; l < MAX_LEVELS; l++) {
        max_saving_node_ids[l] = UINT32_MAX;
    }

    for (uint32_t l = 0; l < graph.total_levels; l++) {
        graph.first_node_of_level[l] = UINT32_MAX;
    }

    graph.first_node_of_level[0] = 0;
    write_idx = 1;

    double max_saving_per_level[MAX_LEVELS] = {0};

    for (uint32_t read_idx = 1; read_idx < graph.size; read_idx++) {
        GraphNode *node = &graph.nodes[read_idx];

        if (node->useless) continue;

        if (node->node_level > current_level) {
            for (uint32_t l = current_level + 1; l <= node->node_level; l++) {
                graph.first_node_of_level[l] = write_idx;
            }
            current_level = node->node_level;
        }

        graph.nodes[write_idx] = *node;
        GraphNode *new_node = &graph.nodes[write_idx];
        new_node->node_id = write_idx;

        // ---- Compute savings for max tracking ----
        uint32_t node_freq = 0, dummy_id;
        if (seq_freq_get(&block[new_node->offset], new_node->sequence_length, &node_freq, &dummy_id)) {
            double saving = calc_savings(new_node, node_freq);
            uint16_t level = new_node->node_level;
            if (saving > max_saving_per_level[level]) {
                max_saving_per_level[level] = saving;
                max_saving_node_ids[level] = new_node->node_id;
            }
        }

        write_idx++;
    }

    printf("%lu: Done with graph compaction from %u to %u nodes\n", get_elapsed_ms(), graph.size, write_idx);
    graph.size = write_idx;
    graph.total_levels = current_level + 1;
}



void compute_best_savings_all(const uint8_t *block, const uint32_t *max_saving_node_ids, uint32_t *best_savings_node_ids) {
    for (uint16_t level = 0; level < graph.total_levels; level++) {
        uint32_t start = get_level_start_id(level);
        uint32_t end   = get_level_end_id(level);

        double max_saving = -1.0;
        best_savings_node_ids[level] = UINT32_MAX;

        for (uint32_t i = start; i < end; i++) {
            GraphNode *node = &graph.nodes[i];
            if (node->useless) {
                node->best_savings = 0;
                continue;
            }

            uint32_t freq = 0, dummy_id;
            if (!seq_freq_get(&block[node->offset], node->sequence_length, &freq, &dummy_id)) {
                node->best_savings = 0;
                continue;
            }

            double own_saving = calc_savings(node, freq);
            double inherited_saving = 0;

            if (level > 0) {
                uint16_t parent_level = get_parent_level(node);
                if (max_saving_node_ids[parent_level] != UINT32_MAX) {
                    GraphNode *parent = get_graph_node(max_saving_node_ids[parent_level]);
                    inherited_saving = parent->best_savings;
                }
            }

            node->best_savings = (uint32_t)(own_saving + inherited_saving);

            if ((double)node->best_savings > max_saving) {
                max_saving = (double)node->best_savings;
                best_savings_node_ids[level] = node->node_id;
            }
        }
    }
}


void find_best_saving_path(const uint8_t *block, uint16_t starting_level) {
    uint32_t max_saving_node_ids[MAX_LEVELS];
    uint32_t best_savings_node_ids[MAX_LEVELS];

    // Step 1: Run graph compaction and savings computation
    compact_graph_with_savings(block, max_saving_node_ids);
    compute_best_savings_all(block, max_saving_node_ids, best_savings_node_ids);

    // Step 2: Initialize best path
    path_init();
    uint16_t level = starting_level;

    while (level < MAX_LEVELS) {
        uint32_t node_id = best_savings_node_ids[level];
        if (node_id == UINT32_MAX) break;

        GraphNode *node = get_graph_node(node_id);
        if (!node) break;

        uint32_t freq = 1, dummy_id;
        double saving = 0;
        if (seq_freq_get(&block[node->offset], node->sequence_length, &freq, &dummy_id)) {
            saving = calc_savings(node, freq);
        }

        int idx = ++path_state.path_size[PATH_BEST];
        path_state.path_stack[PATH_BEST][idx] = node_id;
        path_state.path_per_node_savings[PATH_BEST][idx] = saving;
        path_state.path_freqs[PATH_BEST][idx] = freq;
        path_state.path_total_saving[PATH_BEST] += saving;
        path_state.path_total_freq[PATH_BEST] += freq;

        // Boost frequency to discourage re-use
        if (node->sequence_length > 1 && !node->is_RLE) {
            uint32_t index = seq_freq_get_with_index(&block[node->offset], node->sequence_length, &freq, &dummy_id);
            seq_freq_set_existing(index, freq + 3, node_id);
        }

        if (node->node_id == 0) break; // root node reached
        level = get_parent_level(node);
    }
}

