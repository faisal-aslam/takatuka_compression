// shortest_path_dp.c
//
// Drop-in replacement for shortest_path_greedy.c's find_best_saving_path().
// Same signature, same Graph/Path structures, same calc_cost()/calc_savings()
// cost model from shortest_path_common.h -- only the SEARCH algorithm changes.
//
// -----------------------------------------------------------------------
// Why this is optimal where greedy is not
// -----------------------------------------------------------------------
// The greedy algorithm repeatedly picks the single globally-best pattern,
// freezes every occurrence of it, and repeats on the residual. That is an
// iterative, irreversible sequence of choices: once a pattern is frozen,
// no later round can "undo" it even if a different combination would have
// been cheaper overall. That is exactly what makes it a heuristic, not an
// optimal algorithm.
//
// This file instead computes the TRUE minimum-cost path in one forward
// dynamic-programming sweep over levels. The key structural fact that makes
// this both correct and cheap: every node's parent level is determined
// purely by (node_level - sequence_length) -- i.e. which level a node can
// be reached FROM depends only on the node itself, never on which specific
// path was used to reach that parent level. That means the minimum cost to
// reach level l is a single well-defined number, dp_cost[l], satisfying:
//
//     dp_cost[0] = 0
//     dp_cost[l] = min over every useful node n at level l of:
//                      dp_cost[ l - n->sequence_length ] + calc_cost(n, freq(n))
//
// This is exactly the same "collapse to one state per level" argument
// proven in doc/writeup.tex (Theorem 6.2 / Section 7): a DAG whose edges
// strictly increase in level can be solved by ONE pass in increasing level
// order, no backtracking, no exponential path enumeration, and the result
// is PROVABLY the global optimum for the given cost model -- not an
// approximation of it.
//
// -----------------------------------------------------------------------
// The one thing this requires to stay valid: frequency must be static
// -----------------------------------------------------------------------
// calc_cost() takes a `frequency` argument. For the DP argument above to
// hold, that frequency must be a fixed property of the node itself (e.g.
// "how many times does this exact byte sequence occur across the whole
// compacted graph"), NOT something that changes depending on which path
// is chosen (e.g. "how many times has THIS path already used it"). This
// file always reads frequency via a static seq_freq_get() lookup against
// the already-built map -- it never increments/decrements the map while
// searching -- so every node's cost is a fixed number, and the DP above is
// exact. (This matters most while the cost model is still the dictionary-
// style one; once back-references replace the dictionary, cost naturally
// becomes a pure function of (length, distance) and this concern goes away
// entirely.)
//
// -----------------------------------------------------------------------
// Complexity
// -----------------------------------------------------------------------
// O(total number of nodes in the compacted graph) time, O(MAX_LEVELS) extra
// space for the dp_cost[]/dp_best_node[] tables -- no exponential blow-up,
// no brute-force search, no early-exit-after-first-path bugs.

#include "shortest_path_common.h"
#include "timer.h"
#include "lzss_cost.h"
#include <stdint.h>

// Only one shortest_path_*.c should ever be linked in at a time (see
// used_sources.mk), so file-scope statics here are safe.
static uint64_t dp_cost[MAX_LEVELS];      // dp_cost[l]      = min cost to reach level l
static uint32_t dp_best_node[MAX_LEVELS]; // dp_best_node[l] = node achieving that min
static uint8_t  dp_reached[MAX_LEVELS];   // whether dp_cost[l] has been assigned yet

// Global mode switch (declared extern in lzss_cost.h so future graph-
// construction / matchfinder code can flip it). 0 = today's dictionary-
// style calc_cost() model (default, matches the benchmarked behavior).
// Set to 1 once GraphNode->distance is genuinely being populated by an
// LZSS-style matchfinder -- at that point BOTH literal and match costs
// switch to the consistent bit-based model in lzss_cost.h.
uint8_t use_lzss_cost_model = 0;

/**
 * Cost of using `node` as the edge into its level. Two modes:
 *
 *  - use_lzss_cost_model == 0 (today's default): the existing dictionary-
 *    style calc_cost(), read via a STATIC frequency lookup (never
 *    incremented) so the result depends only on the node itself.
 *
 *  - use_lzss_cost_model == 1: pure (length, distance) LZSS costing from
 *    lzss_cost.h. Needs node->distance to have been set by the graph-
 *    construction / matchfinder code for real back-reference nodes.
 */
static inline uint32_t dp_node_cost(const uint8_t *block, GraphNode *node) {
    if (node->node_id == 0) return 0; // root, no cost

    if (use_lzss_cost_model) {
        if (node->RLE_type) return 1u; // RLE stays its own cheap mechanism
        if (node->sequence_length == 1) return lzss_literal_cost_bits();
        return lzss_match_cost_bits(node->sequence_length, node->distance);
    }

    uint32_t freq = 1; // safe default: "seen once" (matches calc_cost's own
                        // handling of never-repeated sequences; avoids ever
                        // passing freq=0, which calc_cost does not handle).
    if (node->sequence_length > 1 && !node->RLE_type) {
        uint32_t dummy_node_id;
        seq_freq_get(&block[node->offset], node->sequence_length, &freq, &dummy_node_id);
        // If not found, freq stays 1 -- shouldn't normally happen post-
        // compaction (single-occurrence nodes were already pruned), but
        // default safely rather than relying on that invariant.
    }
    return calc_cost(node, freq);
}

/**
 * One forward sweep, level 0 .. last_level. Levels are visited in
 * increasing order and every edge strictly decreases in level when
 * looking backwards (parent_level = level - sequence_length < level), so
 * by the time a node at `level` is relaxed, dp_cost[parent_level] is
 * already final -- the standard DAG shortest-path argument.
 */
static void run_dp(const uint8_t *block) {
    uint16_t last_level = get_last_level_index();

    for (uint16_t l = 0; l <= last_level; l++) dp_reached[l] = 0;
    dp_cost[0] = 0;
    dp_best_node[0] = 0; // root
    dp_reached[0] = 1;

    for (uint16_t level = 1; level <= last_level; level++) {
        if (level >= graph.total_levels) break;

        uint32_t start_id = get_level_start_id(level);
        uint32_t end_id = get_level_end_id(level);
        if (start_id == end_id) continue; // empty level (mid-RLE-run); nothing
                                           // ever points back into it either

        for (uint32_t nid = start_id; nid < end_id; nid++) {
            GraphNode *node = get_graph_node(nid);
            if (node->node_level != level) continue; // safety
            if (node->useless) continue;             // pruned during compaction

            uint16_t parent_level = get_parent_level(node);
            if (!dp_reached[parent_level]) continue; // shouldn't happen; be safe

            uint64_t candidate = dp_cost[parent_level] + dp_node_cost(block, node);

            if (!dp_reached[level] || candidate < dp_cost[level]) {
                dp_cost[level] = candidate;
                dp_best_node[level] = node->node_id;
                dp_reached[level] = 1;
            }
        }
    }
}

/**
 * Walk dp_best_node[] backward from the last level to the root, in the
 * SAME order greedy's add_best_path() produces: index 0 = node at the
 * last level, increasing indices walk toward the root, final index =
 * root (node_id 0). Keeps every downstream consumer (final_book_keeping,
 * print_path, set_best_path_view, ...) working unchanged.
 */
static void reconstruct_path(Path *path_state) {
    uint16_t level = get_last_level_index();

    while (1) {
        uint32_t node_id = dp_best_node[level];
        int32_t idx = ++path_state->path_size[PATH_BEST];
        CHECK_INDEX(idx, "reconstruct_path");
        path_state->path_stack[PATH_BEST][idx] = node_id;

        if (node_id == 0) break; // reached root

        GraphNode *node = get_graph_node(node_id);
        level = get_parent_level(node);
    }
}

void find_best_saving_path(const uint8_t *block, Path *path_state) {
    path_init(path_state);

    // Make sure the map reflects the current (already-compacted) graph.
    // Idempotent / cheap relative to the rest of the pipeline; safe to
    // call even if the caller already did this.
    rebuild_seq_freq_map(block, 0);

    run_dp(block);
    reconstruct_path(path_state);

    uint16_t last_level = get_last_level_index();
    path_state->path_total_cost[PATH_BEST] =
        dp_reached[last_level] ? (uint32_t)dp_cost[last_level] : UINT32_MAX;

    printf("%lu: DP shortest path done. total_cost=%u, path_size=%d\n", get_elapsed_ms(),
           path_state->path_total_cost[PATH_BEST], path_state->path_size[PATH_BEST] + 1);
}
