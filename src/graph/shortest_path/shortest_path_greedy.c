// shortest_path_greedy.c

#include "general_map.h"
#include "shortest_path_brute_force.h"
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
    printf("\nlen=%u\n", len);
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
        GraphNode *current_node = get_graph_node(level_id);
        if (level_id == only_useful_node || current_node->is_RLE) continue;
        current_node->useless = 1; // mark it useless.
    }
}

static inline uint8_t mark_all_but_done_level_deleted() {

    uint8_t contain_not_done_levels = 0;
    // root level is always active and should never be deleted.
    level_status[0] = LEVEL_DONE_OLD;
    // for rest of the levels.
    for (uint16_t level = 1; level <= get_last_level_index(); level++) {
        uint8_t level_nodes = get_level_end_id(level) - get_level_start_id(level);
        if (level_status[level] == LEVEL_ACTIVE) {

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
        if (level_status[level] == LEVEL_DONE_NOW) {
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

static void print_levels_status(void) {
#ifdef DEBUG
    uint16_t last_level = get_last_level_index();
    printf("\n\n");
    for (uint16_t level = 0; level <= last_level; level++) {
        const char *status_str = NULL;

        switch (level_status[level]) {
        case LEVEL_ACTIVE:
            status_str = "ACTIVE";
            break;
        case LEVEL_DELETED:
            status_str = "DELETED";
            break;
        case LEVEL_DONE_NOW:
            status_str = "DONE_NOW";
            break;
        case LEVEL_DONE_OLD:
            status_str = "DONE_OLD";
            break;
        default:
            status_str = "UNKNOWN";
            break;
        }

        printf("level %u = %s\n", level, status_str);
    }
#endif    
}

void find_best_saving_path(const uint8_t *block, Path *path_state) {

    // step 1: Find best sequence.
    const uint8_t *best_seq;
    uint8_t best_len;
    uint32_t best_freq, best_node_id;
    print_levels_status();
    // start by setting all levels to deleted by default.
    while (seq_freq_get_best(&best_seq, &best_len, &best_freq, &best_node_id)) {
        mark_all_but_done_level_deleted();

        print_sequence(best_seq, best_len);

        print_levels_status();

        // Step 2: Go through the graph level by leve. Each level that contains the best sequence is marked done, the
        // perticualr node that contain that sequence is marked useful whereas rest of the nodes of that level are
        // marked useless. All the intermediate level from the best node to its parent node are marked deleted.
        for (uint16_t level = get_last_level_index(); level > 0 && level <= get_last_level_index(); level--) {
            uint32_t start_id = get_level_start_id(level);
            uint32_t end_id = get_level_end_id(level);
            for (uint32_t level_id = start_id; level_id < end_id; level_id++) {
                GraphNode *node = get_graph_node(level_id);
                if (node->useless) continue;
                if (best_len == node->sequence_length && sequences_equal(&block[node->offset], best_seq, best_len)) {
                    // found it.
                    level_status[level] = LEVEL_DONE_NOW;
                    mark_all_but_one_useless(node->node_id);
                    // all the intermediate level from the best node to its parent node are marked deleted.
                    for (int loop = 1; loop < best_len; loop++) {
                        if (level - loop < level && level != 0) {
                            level_status[level - loop] = LEVEL_DELETED;
                        }
                    }
                    level = get_parent_level(node);
                    break;
                }
            }
        }

        print_levels_status();
        uint16_t last_done_level = UINT16_MAX;
        // Step 3: All the nodes below a LEVEL_DONE_NOW cannot have parent that is above the done level.
        // Thus, we cannot bypass the done level.
        for (uint16_t level = get_last_level_index(); level > 0; level--) {
            if (level_status[level] != LEVEL_DONE_NOW) continue;
            uint16_t child_counter = 0;
            
            //save the last done level which is Needed in step 5.
            if (last_done_level == UINT16_MAX) last_done_level = level; 

            //found a done now level. Now make sure none of its child is bypassing it.
            for (uint16_t child_level = level + 1; child_level <= get_last_level_index(); child_level++) {
                child_counter++;
                if (level_status[child_level] == LEVEL_DONE_NOW) break;
                uint32_t start_id = get_level_start_id(child_level);
                uint32_t end_id = get_level_end_id(child_level);
                for (uint32_t level_id = start_id; level_id < end_id; level_id++) {
                    GraphNode *node = get_graph_node(level_id);
                    if (node->sequence_length > child_counter) node->useless = 1;
                }
            }
        }

        print_levels_status();

        // Step 4: All levels which are reachable via latest done levels (i.e. their ancestors) are marked active.
        // This ensures that the graph remains connected.
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
        print_levels_status();

        //Step 5: Finally, all the children of the last done level are mark active so that graph remain connected.
        for (int level = get_last_level_index(); level > last_done_level; level--) {
            level_status[level] = LEVEL_ACTIVE;
        }

        print_levels_status();
        // rebuild the map without done levels.
        compact_graph(block);
        visualize_graph(block);
        rebuild_seq_freq_map(block);

        // break;
    }
    printf("\n\n Compacting and making graph\n");
    compact_graph(block);
    visualize_graph(block);
    init_seq_freq_map(); // initialize the sequence map.
    find_best_saving_path_to_a_node(block, get_last_level_index(), 0, path_state);
}