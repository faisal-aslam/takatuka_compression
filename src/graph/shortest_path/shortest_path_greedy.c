// shortest_path_greedy.c

#include "general_map.h"
#include "shortest_path_brute_force.h"
#include "timer.h"
#include <math.h>
#include <stdbool.h>

uint8_t done_with_RLE_nodes;

// ensure LEVEL_ACTIVE stays 0
_Static_assert(LEVEL_ACTIVE == 0, "LEVEL_ACTIVE must be zero for memset to work");

// call this before building the graph each run
static inline void reset_level_status(void) { memset(level_status, 0, MAX_LEVELS * sizeof(level_status[0])); }

/**
 * Greedy Shortest Path Selection
 *
 * High-level idea:
 *  - Iteratively choose the sequence with the best saving (freq * length).
 *  - Freeze any levels that contain that sequence (one useful node kept).
 *  - Prevent bypass of frozen levels by pruning longer jumps.
 *  - Compact and rebuild frequency map between iterations.
 */

static inline void print_sequence(const uint8_t *seq, uint8_t len) {
    printf("len=%u, [", len);
    for (uint8_t i = 0; i < len; i++) {
        printf("%c", seq[i]);
    }
    printf("]\n");
}

/**
 * For the given level, mark every node useless except the one specified.
 * RLE nodes are skipped (left as-is).
 */
inline static void mark_all_but_one_useless(uint32_t only_useful_node) {
    GraphNode *node = get_graph_node(only_useful_node);
    uint16_t level = node->node_level;
    uint32_t start_id = get_level_start_id(level);
    uint32_t end_id = get_level_end_id(level);
    for (uint32_t level_id = start_id; level_id < end_id; level_id++) {
        GraphNode *current_node = get_graph_node(level_id);
        if (current_node->node_level != level) continue;
        if (level_id == only_useful_node || current_node->RLE_type) {
            current_node->useless = 0; // only usefull
        } else {                       // do not mark rle nodes uselss.
            current_node->useless = 1; // all other are useless.
        }
    }
}

/**
 * Centralized status updater with allowed transitions:
 *  - LEVEL_DONE: immutable, no further changes.
 *  - LEVEL_ACTIVE: May become either LEVEL_DELETED or LEVEL_DONE.
 *  - LEVEL_DELETED: immutable, no further changes.
 *  - The function must abort and give errors if incorrect updates are tried.
 */
static inline void update_level_status(uint16_t level, LevelStatus new_status) {
    LevelStatus old_status = level_status[level];

    switch (old_status) {
    case LEVEL_DONE:
    case LEVEL_DELETED:
        // Immutable states, cannot be changed
        if (new_status != old_status) {
            fprintf(stderr, "update_level_status: invalid transition from %d to %d at level=%u\n", old_status,
                    new_status, level);
            abort();
        }
        return;

    case LEVEL_ACTIVE:
        if (new_status == LEVEL_DELETED || new_status == LEVEL_DONE) {
            level_status[level] = new_status;
            return;
        }
        fprintf(stderr, "update_level_status: invalid transition from ACTIVE(%d) to %d at level=%u\n", old_status,
                new_status, level);
        abort();

    default:
        fprintf(stderr, "update_level_status: invalid old_status=%d at level=%u\n", old_status, level);
        abort();
    }
}

static void print_levels_status(void) {
    if (1) return;
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
        case LEVEL_DONE:
            status_str = "DONE";
            break;
        default:
            status_str = "UNKNOWN";
            break;
        }
        printf("level %u = %s\n", level, status_str);
    }
#endif
}

uint8_t get_best_saving(const uint8_t *block, uint8_t *best_seq, uint8_t *best_len, uint32_t *best_freq) {
    uint32_t best_node_id = 0;
    uint8_t longest_RLE = 0;
    uint16_t longest_RLE_level = 0;
    if (!done_with_RLE_nodes) {
        // Step 1: scan levels for the longest RLE
        for (uint16_t level = get_last_level_index(); level > 0 && level <= get_last_level_index(); level--) {
            if (level_status[level] != LEVEL_ACTIVE) // only in active levels.
                continue;

            uint32_t start_id = get_level_start_id(level);
            uint32_t end_id = get_level_end_id(level);
            if (start_id == end_id) continue; // empty level.
            GraphNode *node = get_graph_node(start_id);

            if (node->RLE_type && node->sequence_length > longest_RLE) {
                longest_RLE = node->sequence_length;
                longest_RLE_level = level;
                best_node_id = start_id;
            }
        }

        // Step 2: If we found an RLE candidate, return it
        if (longest_RLE_level != 0) {
            GraphNode *node = get_graph_node(best_node_id);
            memcpy(best_seq, &block[node->offset], node->sequence_length);
            *best_len = node->sequence_length;
            *best_freq = 2; // TODO: real frequency
            return 1;
        } else {
            done_with_RLE_nodes = 1;
        }
    }

    // Step 3: Otherwise, fallback to normal best sequence
    const uint8_t *seq_ptr = NULL;
    // map is recreated only using active levels.
    rebuild_seq_freq_map(block, 1);
    uint8_t found = seq_freq_get_best(&seq_ptr, best_len, best_freq, &best_node_id);
    if (found) {
        memcpy(best_seq, seq_ptr, *best_len); // copy into caller buffer
    }
    return found;
}

// Prevent bypass of DONE_NOW by pruning children that would jump past it.
// If a child at 'child_level' needs > child_counter symbols to reach the parent,
// it would skip the frozen level; mark such nodes useless.
static void avoid_done_level_skipping(uint16_t level) {
    for (uint16_t child_level = level + 1; child_level <= get_last_level_index(); child_level++) {
        uint32_t start_id = get_level_start_id(child_level);
        uint32_t end_id = get_level_end_id(child_level);
        for (uint32_t level_id = start_id; level_id < end_id; level_id++) {
            GraphNode *node = get_graph_node(level_id);
            if (get_parent_level(node) < level) { // the parent is bypassing done node which is not allowed.
                node->useless = 1;
            }
        }
    }
}

static inline void path_init_best(Path *path_state) {
    path_state->path_size[PATH_BEST] = -1;
    path_state->path_total_saving[PATH_BEST] = 0;
    path_state->path_total_freq[PATH_BEST] = 0;
    path_state->path_total_cost[PATH_BEST] = 0;
}

void add_best_path(const uint8_t *block, Path *path_state) {
    uint16_t level = get_last_level_index();
    while (1) {        
        GraphNode *node = get_graph_node(get_level_start_id(level));
        if (get_level_end_id(level) - get_level_start_id(level) != 1) {
            fprintf(stderr, "illegal path\n");
            abort();
        }
        //printf("level =%u, seq=", level);
        //print_node_sequence(node, block);
        //printf("\n");
        path_state->path_stack[PATH_BEST][++path_state->path_size[PATH_BEST]] = node->node_id;        
        if (node->node_id == 0) break; //we are done.
        level = get_parent_level(node);
    }
}

/**
 * Main greedy iteration loop:
 *  - Each iteration freezes at least one level (DONE_NOW → DONE_OLD).
 *  - Between iterations the graph is compacted and the seq map is rebuilt.
 */
void find_best_saving_path(const uint8_t *block, Path *path_state) {

    uint8_t best_seq[SEQ_LENGTH_LIMIT];
    uint8_t best_len;
    uint32_t best_freq;
    done_with_RLE_nodes = 0;
    reset_level_status(); // reset level status.
    print_levels_status();

    while (get_best_saving(block, best_seq, &best_len, &best_freq) && best_freq > 1) {
        // Default everything (except root / done) to DELETED, rotate DONE_NOW → DONE_OLD

#ifdef DEBUG
        seq_freq_map_print();
        printf("freq=%u, ", best_freq);
        print_sequence(best_seq, best_len);
        fflush(stdout);
#endif

        uint16_t found_count = 0;
        print_levels_status();

        // 1. Freeze levels that contain the best sequence, keeping only that node useful.
        // 2.  Also delete intermediate levels between a frozen level and its parent.
        // 3. Do not allow to bypass the frozen level by one of its children
        for (uint16_t level = get_last_level_index(); level > 0 && level <= get_last_level_index(); level--) {
            if (level_status[level] != LEVEL_ACTIVE) continue; // only use active levels.
            uint32_t start_id = get_level_start_id(level);
            uint32_t end_id = get_level_end_id(level);

            for (uint32_t level_id = start_id; level_id < end_id; level_id++) {
                GraphNode *node = get_graph_node(level_id);
                if (node->useless) continue;

                if (best_len == node->sequence_length && sequences_equal(&block[node->offset], best_seq, best_len)) {
                    found_count++;
                    // Freeze this level now.
                    update_level_status(level, LEVEL_DONE);
                    mark_all_but_one_useless(node->node_id);

                    // Mark intermediate levels (between this level and its parent) as DELETED.
                    uint16_t parent_level = get_parent_level(node);
                    for (uint16_t loop = level - 1; loop > parent_level; loop--) {
                        update_level_status(loop, LEVEL_DELETED);
                    }
                    avoid_done_level_skipping(level);
                    // Jump to parent level for the next outer-iteration step.
                    level = parent_level + 1; // as there will be -- at the end of loop so compensate it here.
                    break;
                }
            }
        }

        // #ifdef DEBUG
        printf("best_freq=%u, best_len=%u, found_count=%u\n", best_freq, best_len, found_count);
        // #endif
        print_levels_status();

#ifdef DEBUG
        visualize_graph(block);
#endif
    }
    mark_single_freq_nodes_useless(block);
    compact_graph(block);
    visualize_graph(block);
    init_seq_freq_map();
    add_best_path(block, path_state);
}
