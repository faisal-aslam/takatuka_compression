// shortest_path_greedy.c

#include "shortest_path_brute_force.h"
#include "general_map.h"
#include "timer.h"
#include <math.h>
#include <stdbool.h>


/**
 * Greedy Shortest Path Selection
 *
 * Step 1: Identify the sequence `b` with the maximum saving
 *         (saving = frequency × length) among all sequences
 *         in levels that are not yet processed.
 *
 * Step 2: For every level containing `b`, mark all nodes as
 *         useless except `b` itself. Mark that level as done.
 *
 * Step 3: For all levels that can reach the lowest-level
 *         occurrence of `b` from the sink, mark them as useful.
 *
 * Step 4: For all ancestor levels of `b` (levels that are ancestor/parent a b's
 *         level), mark them as useful.
 *
 * Step 5: Any nodes and levels not marked useful are considered
 *         useless and are removed from the graph.
 *
 * Step 6: Repeat from Step 1 while there are still levels
 *         remaining that are not marked done, in the trimmed graph.
 */

static inline void print_sequence(const uint8_t *seq, uint8_t len) {
    for (int i = 0; i < len; i++) {
        printf("%c", seq[i]);
    }
    printf("\n");
}

inline static void mark_all_but_one_useless(uint32_t only_useful_node) {
    GraphNode *node = get_graph_node(only_useful_node);
    uint16_t level = node->node_level;
    uint32_t start_id = get_level_start_id(level);
    uint32_t end_id = get_level_end_id(level);
    for (uint32_t level_id = start_id; level_id < end_id; level_id++) {
        if (level_id == only_useful_node) continue;
        GraphNode *current_node = get_graph_node(level_id);
        current_node->useless = 1; // mark it useless.
    }
}

static inline uint8_t mark_all_but_done_level_deleted() {
    // skip root level 0
    uint8_t contain_not_done_levels = 0;
    for (uint16_t level = 1; level <= get_last_level_index(); level++) {
        uint8_t level_nodes = get_level_end_id(level) - get_level_start_id(level);
        if (level_status[level] == LEVEL_ACTIVE && level_nodes > 1) {

            level_status[level] = LEVEL_DELETED;
            contain_not_done_levels = 1;
        }
        if (level_status[level] ==
            LEVEL_DONE_NOW) { // move done now to done old so that latest done can be differentiated.
            level_status[level] = LEVEL_DONE_OLD;
        }
    }
    return contain_not_done_levels;
}

static inline void add_done_levels_to_process(uint8_t *level_to_process) {
    // skip root level 0
    for (uint16_t level = 1; level <= get_last_level_index(); level++) {
        if (level_status[level] == LEVEL_DONE_NOW /*|| level_status[level] == LEVEL_DONE_OLD*/) {
            level_to_process[level] = 1;
        } else {
            level_to_process[level] = 0;
        }
    }
}

static void rebuild_seq_freq_map(const uint8_t *block) {
    init_seq_freq_map(); // start fresh

    for (uint32_t i = 0; i < graph.size; i++) {
        GraphNode *node = &graph.nodes[i];
        if (level_status[node->node_level] == LEVEL_DONE_NOW || level_status[node->node_level] == LEVEL_DONE_OLD)
            continue;                             // skip done levels.
        if (node->useless) continue;              // skip useless nodes
        if (node->sequence_length <= 1) continue; // skip trivial sequences
        if (node->is_RLE) continue;               // skip RLE nodes

        seq_freq_increment(&block[node->offset], node->sequence_length, node->node_id);
    }
}

void find_best_saving_path(const uint8_t *block, Path *path_state) {

    // start by setting all levels to deleted by default.
    while (mark_all_but_done_level_deleted()) {

        // root level is always active and should never be deleted.
        level_status[0] = LEVEL_DONE_OLD;

        // step 1: Find best sequence.
        const uint8_t *best_seq;
        uint8_t best_len;
        uint32_t best_freq, best_node_id;

        if (seq_freq_get_best(&best_seq, &best_len, &best_freq, &best_node_id)) {
            //print_sequence(best_seq, best_len);
        } else {
            fprintf(stderr, "best sequence does not exist\n");
            break;
        }

        // Step 2: Go through the graph level by leve. Each level that contains the best sequence is marked done, the
        // perticualr node that contain that sequence is marked useful whereas rest of the nodes of that level are
        // marked useless. Step 3: Furthermore, all levels below first such level (done levels) are marked active.
        uint8_t found_once = 0;
        uint16_t last_done_level = UINT16_MAX;
        for (uint16_t level = get_last_level_index(); level > 0; level--) {
            uint32_t start_id = get_level_start_id(level);
            uint32_t end_id = get_level_end_id(level);
            for (uint32_t level_id = start_id; level_id < end_id; level_id++) {
                GraphNode *node = get_graph_node(level_id);
                if (node->useless) continue;
                if (best_len == node->sequence_length && sequences_equal(&block[node->offset], best_seq, best_len)) {
                    // found it.
                    found_once = 1;
                    level_status[node->node_level] = LEVEL_DONE_NOW;
                    mark_all_but_one_useless(node->node_id);
                    if (last_done_level == UINT16_MAX) last_done_level = level;
                    break;
                }
            }
            if (!found_once && level_status[level] == LEVEL_DELETED)
                level_status[level] = LEVEL_ACTIVE; // it is below any best sequence found.
        }

        // Step 4: All levels which are reachable via last done (i.e. their ancestors) are marked active.
        uint8_t level_to_process[MAX_LEVELS] = {0};
        add_done_levels_to_process(level_to_process);

        for (int level = get_last_level_index(); level > 0; level--) {
            if (!level_to_process[level]) continue;
            if (level_status[level] == LEVEL_DELETED) level_status[level] = LEVEL_ACTIVE;
            uint32_t start_id = get_level_start_id(level);
            uint32_t end_id = get_level_end_id(level);
            for (uint32_t level_id = start_id; level_id < end_id; level_id++) {
                GraphNode *node = get_graph_node(level_id);
                if (node->useless) continue;
                level_to_process[get_parent_level(node)] = 1;
            }
        }
        // rebuild the map without done levels.
        //compact_graph(block);
        //visualize_graph(block);
        rebuild_seq_freq_map(block);

        // break;
    }
    printf("\n\n Compacting and making graph\n");
    //compact_graph(block);
    //visualize_graph(block);
    init_seq_freq_map(); // initialize the sequence map.
    find_best_saving_path_to_a_node(block, get_last_level_index(), 0, path_state);
}