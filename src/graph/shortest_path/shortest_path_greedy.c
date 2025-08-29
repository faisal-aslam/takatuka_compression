// shortest_path_greedy.c

#include "seq_freq_map.h"
#include "graph.h"
#include "constants.h"
#include "shortest_path.h"
#include "timer.h"
#include "general_map.h"

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
    GraphNode* node = get_graph_node(only_useful_node);
    uint16_t level = node->node_level;
    uint32_t start_id = get_level_start_id(level);
        uint32_t end_id = get_level_end_id(level);
        for (uint32_t level_id=start_id; level_id < end_id; level_id++) {
            if (level_id == only_useful_node) continue;
            GraphNode* current_node = get_graph_node(level_id);
            current_node->useless = 1; //mark it useless.
        }
}
void find_best_saving_path(const uint8_t *block, Path *path_state) {   

    level_status[0] = LEVEL_ACTIVE; //root level is always active.
    //step 1: Find best sequence.
    const uint8_t *best_seq;
    uint8_t best_len;
    uint32_t best_freq, best_node_id;

    if (seq_freq_get_best(&best_seq, &best_len, &best_freq, &best_node_id)) {
        print_sequence(best_seq, best_len);
    } else {
        fprintf(stderr, "best sequence does not exist\n");
        abort();
    }

    //Step 2: Go through the graph level by leve. Each level that contains the best sequence is marked done, the perticualr
    //node that contain that sequence is marked useful whereas rest of the nodes of that level are marked useless.
    //Step 3: Furthermore, all levels below first such level (done levels) are marked active.
    uint8_t found_once = 0;
    uint16_t last_done_level = UINT16_MAX;
    for (uint16_t level=get_last_level_index(); level > 0; level--) {
        uint32_t start_id = get_level_start_id(level);
        uint32_t end_id = get_level_end_id(level);
        for (uint32_t level_id=start_id; level_id < end_id; level_id++) {
            GraphNode* node = get_graph_node(level_id);
            if (node->useless) continue;
            if (best_len == node->sequence_length && sequences_equal(&block[node->offset], best_seq, best_len)) {
                //found it.
                found_once = 1;
                level_status[node->node_level] = LEVEL_DONE;
                mark_all_but_one_useless(node->node_id);
                if (last_done_level == UINT16_MAX) last_done_level = level;
                break;
            }
        }
        if (!found_once) level_status[level] = LEVEL_ACTIVE; //it is below any best sequence found.
    } 

    //Step 4: All levels which are reachable via last done (i.e. their ancestors) are marked active.    
    uint16_t level_to_process[MAX_LEVELS];
    uint16_t level_to_process_size = 0;
    uint16_t level_to_process_current = 0;
    level_to_process[level_to_process_size++] = last_done_level;    
    while (level_to_process_size < level_to_process_current) {
        uint16_t level = level_to_process[level_to_process_current];
        uint32_t start_id = get_level_start_id(level);
        uint32_t end_id = get_level_end_id(level);
        for (uint32_t level_id=start_id; level_id < end_id; level_id++) {
            GraphNode *node = get_graph_node(level_id);
            if (node->useless) continue;
            if (level_status[node->node_level] == LEVEL_DELETED) level_status[node->node_level] = LEVEL_ACTIVE;
            level_to_process[level_to_process_size++] = get_parent_level(node);
        }
    }
    visualize_graph(block);

}