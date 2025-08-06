// shortest_path_less_greedy.c

#include "shortest_path_common.h"
#include <float.h>
#include <math.h>
#include <stdbool.h>

Path path_state;
uint32_t max_saving_node_ids[MAX_LEVELS];
void compute_best_savings_all(const uint8_t *block, const uint32_t *max_saving_node_ids,
                              uint32_t *best_savings_node_ids) {
    for (uint16_t level = 0; level < graph.total_levels; level++) {
        uint32_t start = get_level_start_id(level);
        uint32_t end = get_level_end_id(level);

        double max_saving = -1.0;
        best_savings_node_ids[level] = UINT32_MAX;

#ifdef DEBUG
        printf("\n[Level %u] start=%u, end=%u\n", level, start, end);
#endif

        for (uint32_t i = start; i < end; i++) {
            GraphNode *node = &graph.nodes[i];

            if (node->useless) {
                node->best_savings = 0;
#ifdef DEBUG
                //printf("  Node %u: useless -> best_savings = 0\n", node->node_id);
#endif
                continue;
            }

            uint32_t freq = 1, dummy_id = 0;
            if (!node->is_RLE && node->sequence_length > 1) {
                if (!seq_freq_get(&block[node->offset], node->sequence_length, &freq, &dummy_id)) {
                    freq = 1; // fallback
#ifdef DEBUG
                    printf("  Node %u: seq_freq not found, fallback freq = 1\n", node->node_id);
#endif
                }
            }

            double own_saving = calc_savings(node, freq);
            double inherited_saving = 0.0;

            if (level > 0) {
                uint16_t parent_level = get_parent_level(node);
                if (parent_level < graph.total_levels && max_saving_node_ids[parent_level] != UINT32_MAX) {
                    GraphNode *parent = get_graph_node(max_saving_node_ids[parent_level]);
                    inherited_saving = parent->best_savings;
#ifdef DEBUG
                    printf("  Node %u: inherited %0.2f from parent node %u (level %u)\n",
                        node->node_id, inherited_saving, parent->node_id, parent_level);
#endif
                }
            }

            node->best_savings = (uint32_t)(own_saving + inherited_saving);

#ifdef DEBUG
            printf("  Node %u: freq = %u, own_saving = %0.2f, inherited = %0.2f, total = %u\n",
                   node->node_id, freq, own_saving, inherited_saving, node->best_savings);
#endif

            if ((double)node->best_savings > max_saving) {
                max_saving = (double)node->best_savings;
                best_savings_node_ids[level] = node->node_id;
#ifdef DEBUG
                printf("    --> Node %u becomes best so far with total saving %u\n",
                       node->node_id, node->best_savings);
#endif
            }
        }

#ifdef DEBUG
        if (best_savings_node_ids[level] != UINT32_MAX) {
            printf("[Level %u] Best node: %u with saving %0.2f\n", level,
                   best_savings_node_ids[level], max_saving);
        } else {
            printf("[Level %u] No valid best node found.\n", level);
        }
#endif
    }
}

static inline void update_current_path(GraphNode *node, const uint8_t* block) {
        int idx = ++path_state.path_size[PATH_CURRENT];
        path_state.path_stack[PATH_CURRENT][idx] = node->node_id;
        //add the last level seq in the map.
        uint32_t freq = seq_freq_increment(&block[node->offset], node->sequence_length, node->node_id);
        double savings = calc_savings(node, freq);
        path_state.path_per_node_savings[PATH_CURRENT][idx] = savings; 
        path_state.path_freqs[PATH_CURRENT][idx] = freq;
        path_state.path_total_saving[PATH_CURRENT] += savings;
        path_state.path_total_freq[PATH_CURRENT] += freq;
}



/*
Amont the nodes which are common in the given level and seq map, find the node with the higest saving. 
*/
static uint32_t find_best_in_map(uint16_t level, const uint8_t* block) {
    uint32_t start_id_of_last_level = get_level_start_id(level);
    uint32_t end_id_of_last_level = get_level_end_id(level);
    uint32_t freq=0, map_node_id;
    double best_savings = 0;
    uint32_t best_saving_node_id = UINT32_MAX;
    for (uint32_t id = start_id_of_last_level; id < end_id_of_last_level; id++) {
        GraphNode* node = get_graph_node(id);
        if(seq_freq_get(&block[node->offset], node->sequence_length, &freq, &map_node_id) && freq > 0) {
            double savings = calc_savings(node, freq);
            if (savings > best_savings) {
                best_saving_node_id = node->node_id;
                best_savings = savings;
            }
        }
    }
    return best_saving_node_id;
}

/**
 * It will try all the nodes of the last level. The node it is trying at the moment is put in the hash. 
 * For other level, first it will get the best saving node among the nodes of that level which are in the hash.
 * if no such node is found then it will select the best saving node of that level.
 */
void find_best_saving_path(const uint8_t *block, uint16_t starting_level) {
    
    uint32_t best_savings_node_ids[MAX_LEVELS];

    // Step 1: Run graph compaction and savings computation
    compute_max_saving_node_ids(block, max_saving_node_ids);

    // Aggregated savings of a node and its ancestors.
    compute_best_savings_all(block, max_saving_node_ids, best_savings_node_ids);

#ifdef DEBUG
    fflush(stdout);
    visualize_graph(block);
    fflush(stdout);
#endif    // Step 2: Initialize best path
    path_init();
    uint16_t level = starting_level;
    
    /*uint32_t start_id_of_last_level = get_level_start_id(starting_level);
    uint32_t end_id_of_last_level = get_level_end_id(starting_level);
    for (uint32_t id = start_id_of_last_level; id < end_id_of_last_level; id++) {*/
        uint32_t id = best_savings_node_ids[level];
        //map is cleared before finding a path.
        init_seq_freq_map(); 
        //fetch the node of the last level
        GraphNode* node = get_graph_node(id);
#ifdef DEBUG
        printf("At last level %u selected ", node->node_level);
        print_graph_node(node);
#endif
        update_current_path(node, block);
        //go to the parent level
        level = get_parent_level(node);
         while (level < MAX_LEVELS) {
            //first check if the map has a node with most savings.
            uint32_t most_saving_node_id = find_best_in_map(level, block);
            if (most_saving_node_id == UINT32_MAX) {
                most_saving_node_id = best_savings_node_ids[level];
            }
            node = get_graph_node(most_saving_node_id);
#ifdef DEBUG
        printf("At level %u selected ", node->node_level);
        print_graph_node(node);
#endif

            update_current_path(node, block);
            if (node->node_id == 0) break;
            level = get_parent_level(node);
         }
         update_best_path();
         print_path(0, 1, block);
    //}
    final_book_keeping(block);
    print_path(0, 1, block);
}
