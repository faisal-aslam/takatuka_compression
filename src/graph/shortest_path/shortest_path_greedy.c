// shortest_path_greedy.c

#include "seq_freq_map.h"
#include "graph.h"
#include "constants.h"
#include "shortest_path.h"
#include "timer.h"

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


typedef enum {
    LEVEL_DELETED = 0,   // default state
    LEVEL_ACTIVE,        // normal, nothing special
    LEVEL_DONE           // processed/finished
} LevelStatus;

static LevelStatus level_status[MAX_LEVELS]; // all initialized to LEVEL_DELETED by default

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
    // Furthermore, all levels below first such level (done levels) are marked active.
    for (uint16_t level=get_last_level_index(); level > 0; level--) {
        //= get_level_start_id(level);
    } 

}