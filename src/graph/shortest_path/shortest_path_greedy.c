// shortest_path_greedy.c

#include "seq_freq_map.h"
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
 * Step 3: For all ancestor levels of `b` (levels that are ancestor/parent a b's 
 *         level), mark them as useful.
 *
 * Step 4: For all levels that can reach the lowest-level 
 *         occurrence of `b` from the sink, mark them as useful.
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

 static inline find_best_sequence() {
    const uint8_t *best_seq;
    uint8_t best_len;
    uint32_t best_freq, best_node_id;

    if (seq_freq_get_best(&best_seq, &best_len, &best_freq, &best_node_id)) {
        print_sequence(best_seq, best_len);
    } 
 }