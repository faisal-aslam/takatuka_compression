// shortest_path_greedy.c

#include "general_map.h"
#include "shortest_path_brute_force.h"
#include "timer.h"
#include <math.h>
#include <stdbool.h>

/**
 * Greedy Shortest Path Selection
 *
 * High-level idea:
 *  - Iteratively choose the sequence with the best saving (freq * length).
 *  - Freeze any levels that contain that sequence (one useful node kept).
 *  - Prevent bypass of frozen levels by pruning longer jumps.
 *  - Keep the graph connected by activating ancestors and certain children.
 *  - Compact and rebuild frequency map between iterations.
 */

static inline void print_sequence(const uint8_t *seq, uint8_t len) {
    printf("\nlen=%u\n", len);
    for (uint8_t i = 0; i < len; i++) {
        printf("%c", seq[i]);
    }
    printf("\n");
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
        if (level_id == only_useful_node || current_node->is_RLE) {
            current_node->useless = 0; //only usefull
        } else { //do not mark rle nodes uselss.
            current_node->useless = 1; //all other are useless.
        }
    }
}

/**
 * Centralized status updater with allowed transitions:
 *  - LEVEL_DONE_OLD: immutable, no further changes.
 *  - LEVEL_DONE_NOW: may transition only to LEVEL_DONE_OLD.
 *  - LEVEL_ACTIVE / LEVEL_DELETED: may transition to any of the four states.
 */
static inline void update_level_status(uint16_t level, LevelStatus new_status) {
    LevelStatus old_status = level_status[level];

    switch (old_status) {
    case LEVEL_DONE_OLD:
        return; // frozen

    case LEVEL_DONE_NOW:
        if (new_status == LEVEL_DONE_OLD) {
            level_status[level] = LEVEL_DONE_OLD;
        }
        return;

    case LEVEL_ACTIVE:
    case LEVEL_DELETED:
        level_status[level] = new_status;
        return;

    default:
        fprintf(stderr, "update_level_status: invalid old_status=%d for level=%u\n", old_status, level);
        return;
    }
}

/**
 * Marks all levels as deleted by default at the start of an iteration,
 * except:
 *  - root (forced to DONE_OLD)
 *  - levels already DONE_OLD remain so (immutable)
 *  - levels DONE_NOW are rotated to DONE_OLD (freezing them)
 *
 * Returns 1 if at least one non-done level existed (i.e., work remains),
 * otherwise 0.
 */
static uint8_t mark_all_but_done_level_deleted(void) {
    uint8_t contain_not_done_levels = 0;

    // Root is never deleted; treat as permanently frozen.
    update_level_status(0, LEVEL_DONE_OLD);

    for (uint16_t level = 1; level <= get_last_level_index(); level++) {
        LevelStatus s = level_status[level];

        // ACTIVE → DELETED is permitted by the rules
        if (s == LEVEL_ACTIVE) {
            update_level_status(level, LEVEL_DELETED);
            contain_not_done_levels = 1;
        } else if (s == LEVEL_DELETED) {
            contain_not_done_levels = 1;
        } else if (s == LEVEL_DONE_NOW) {
            // DONE_NOW → DONE_OLD (freeze)
            update_level_status(level, LEVEL_DONE_OLD);
        }
        // DONE_OLD stays as-is (immutable)
    }
    return contain_not_done_levels;
}

/**
 * Seed a per-level bit vector with the set of levels that are DONE_NOW.
 * The caller uses this to BFS ancestors upward.
 */
static inline void add_done_levels_to_process(uint8_t *level_to_process) {
    // skip root
    for (uint16_t level = 1; level <= get_last_level_index(); level++) {
        level_to_process[level] = (level_status[level] == LEVEL_DONE_NOW) ? 1u : 0u;
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

/**
 * Main greedy iteration loop:
 *  - Each iteration freezes at least one level (DONE_NOW → DONE_OLD).
 *  - Between iterations the graph is compacted and the seq map is rebuilt.
 */
void find_best_saving_path(const uint8_t *block, Path *path_state) {

    const uint8_t *best_seq;
    uint8_t best_len;
    uint32_t best_freq, best_node_id;

    print_levels_status();

    while (seq_freq_get_best(&best_seq, &best_len, &best_freq, &best_node_id)) {
        // Default everything (except root / done) to DELETED, rotate DONE_NOW → DONE_OLD
        (void)mark_all_but_done_level_deleted();

#ifdef DEBUG
        seq_freq_map_print();
#endif
        print_sequence(best_seq, best_len);
        print_levels_status();

        // Step 2: Freeze levels that contain the best sequence, keeping only that node useful.
        // Also delete intermediate levels between a frozen level and its parent (no bypass).
        for (uint16_t level = get_last_level_index(); level > 0 && level <= get_last_level_index(); level--) {
            uint32_t start_id = get_level_start_id(level);
            uint32_t end_id = get_level_end_id(level);

            for (uint32_t level_id = start_id; level_id < end_id; level_id++) {
                GraphNode *node = get_graph_node(level_id);
                if (node->useless) continue;

                if (best_len == node->sequence_length && sequences_equal(&block[node->offset], best_seq, best_len)) {
                    // Freeze this level now.
                    update_level_status(level, LEVEL_DONE_NOW);
                    mark_all_but_one_useless(node->node_id);

                    // Mark intermediate levels (between this level and its parent) as DELETED.
                    for (uint8_t loop = 1; loop < best_len; loop++) {
                        // avoid underflow; never touch root
                        if (level >= loop && (level - loop) > 0) {
                            update_level_status((uint16_t)(level - loop), LEVEL_DELETED);
                        }
                    }

                    // Jump to parent level for the next outer-iteration step.
                    level = get_parent_level(node);
                    break;
                }
            }
        }

        print_levels_status();

        // Step 3: Prevent bypass of DONE_NOW by pruning children that would jump past it.
        // If a child at 'child_level' needs > child_counter symbols to reach the parent,
        // it would skip the frozen level; mark such nodes useless.
        uint16_t last_done_level = UINT16_MAX;
        for (uint16_t level = get_last_level_index(); level > 0 && level <= get_last_level_index(); level--) {
            if (level_status[level] != LEVEL_DONE_NOW) continue;

            if (last_done_level == UINT16_MAX) last_done_level = level;

            uint16_t child_counter = 0;
            for (uint16_t child_level = level + 1; child_level <= get_last_level_index(); child_level++) {
                child_counter++;
                if (level_status[child_level] == LEVEL_DONE_NOW) break;

                uint32_t start_id = get_level_start_id(child_level);
                uint32_t end_id = get_level_end_id(child_level);
                for (uint32_t level_id = start_id; level_id < end_id; level_id++) {
                    GraphNode *node = get_graph_node(level_id);
                    if (node->sequence_length > child_counter && !node->is_RLE) { //skip RLE nodes. They are darlings.
                        node->useless = 1;
                    }
                }
            }
        }

        print_levels_status();

        // Step 4: Keep graph connected upward:
        // - Start from DONE_NOW levels and mark their ancestors ACTIVE (if currently DELETED).
        // - Use a level-mark array to avoid repeated enqueue.
        uint8_t level_to_process[MAX_LEVELS] = {0};
        add_done_levels_to_process(level_to_process);

        for (int level = (int)get_last_level_index(); level > 0 && level <= get_last_level_index(); level--) {
            if (!level_to_process[level]) continue;

            if (level_status[level] == LEVEL_DELETED) {
                update_level_status((uint16_t)level, LEVEL_ACTIVE);
            }

            uint32_t start_id = get_level_start_id((uint16_t)level);
            uint32_t end_id = get_level_end_id((uint16_t)level);
            for (uint32_t level_id = start_id; level_id < end_id; level_id++) {
                GraphNode *node = get_graph_node(level_id);
                if (node->useless) continue;
                level_to_process[get_parent_level(node)] = 1;
            }
        }

        print_levels_status();

        // Step 5: Also mark all children of the last frozen level ACTIVE
        // so the remaining graph stays connected below.
        if (last_done_level != UINT16_MAX) {
            for (int level = (int)get_last_level_index(); level > (int)last_done_level; level--) {
                update_level_status((uint16_t)level, LEVEL_ACTIVE);
            }
        }

        print_levels_status();
        
        // Prepare next greedy iteration on the trimmed graph.
        compact_graph(block);
        if(graph.size <=24350) break;
#ifdef DEBUG
        visualize_graph(block);
#endif        
        rebuild_seq_freq_map(block, 1);
    }
#ifdef DEBUG
    printf("\n\n Compacting and making graph\n");
    compact_graph(block);
    visualize_graph(block);
#endif    
    init_seq_freq_map();
    find_best_saving_path_to_a_node(block, get_last_level_index(), 0, path_state);
}
