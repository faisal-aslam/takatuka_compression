// shortest_path.c

#include "shortest_path.h"
#include "seq_freq_map.h"
#include <stdbool.h>

int prune_count = 0;

#define CHECK_INDEX(idx, label)                                                                                        \
    if ((idx) < 0 || (idx) >= MAX_LEVELS) {                                                                            \
        fprintf(stderr, "ERROR: Index %d out of bounds in %s (MAX_LEVELS = %d)\n", (idx), (label), MAX_LEVELS);        \
        abort();                                                                                                       \
    }

typedef struct {
    uint32_t node_id;
    uint32_t node_id_popped;
    uint32_t hash_index_cache;
} StackItem;

Path path_state;

/**
 * @brief Calculates the storage cost in bytes for adding a graph node to a path
 *
 * This is a hot path function - optimized for minimal branching and fast execution.
 * All costs are calculated in bytes of storage required.
 *
 * Cost Rules:
 * - Zero-length sequences: 0 bytes (invalid case, handled defensively)
 * - Single-byte sequences: 1 byte (raw byte)
 * - Multi-byte unique sequences (freq=1): n+1 bytes (n bytes + 1 byte length prefix)
 * - Multi-byte repeated sequences (freq>1): 1 byte (reference to dictionary)
 * - RLE sequences: pattern_length + 1 byte (pattern + repeat count)
 *
 * @param node Pointer to graph node being evaluated
 * @param frequency Frequency count of this sequence in the data
 * @return double Storage cost in bytes (always >= 0)
 */
static inline double calc_savings(GraphNode *node, uint32_t frequency) {

    // Branchless design for common cases - reduces pipeline stalls
    const uint8_t len = node->sequence_length;
    double base_cost;

    if (node->node_id == 0) return 0; // no cost for the root node.

    // Handle RLE case first (uses different cost model)
    if (node->is_RLE) {
        // RLE cost: pattern length + 1 byte for repeat count
        return (node->length_of_RLE / node->repeat_seq_length) * 2;
    }

    // Main cost calculation branches
    if (len <= 1) {
        // Cases: 0 bytes = 0 cost, 1 byte = 1 cost
        base_cost = 0;
    } else {
        // Multi-byte case: 1 byte if repeated, n+1 bytes if unique
        base_cost = frequency * node->sequence_length * node->sequence_length;
    }

    return base_cost;
}

/**
 * Initializes the path state for a new search.
 */
static inline void path_init() {
    path_state.path_size[PATH_CURRENT] = -1;
    path_state.path_size[PATH_BEST] = -1;
    path_state.path_total_cost[PATH_CURRENT] = 0;
    path_state.path_total_cost[PATH_BEST] = -1;
    path_state.path_size[PATH_CURRENT] = -1;
    path_state.path_size[PATH_BEST] = -1;
    path_state.path_total_freq[PATH_BEST] = 0;
    path_state.path_total_freq[PATH_CURRENT] = 0;
}

/**
 * Updates the best path if the current path is better.
 */
static inline uint8_t update_best_path() {
    uint8_t ret = 0;

    int32_t cost_current = path_state.path_total_cost[PATH_CURRENT];
    int32_t cost_best = path_state.path_total_cost[PATH_BEST];
    int32_t size_current = path_state.path_size[PATH_CURRENT];
    int32_t size_best = path_state.path_size[PATH_BEST];
    uint32_t freq_current = path_state.path_total_freq[PATH_CURRENT];
    uint32_t freq_best = path_state.path_total_freq[PATH_BEST];

    if (cost_current > cost_best || (cost_current == cost_best && size_current < size_best) ||
        (cost_current == cost_best && size_current == size_best && freq_current > freq_best)) {

        int32_t size = size_current + 1;
        CHECK_INDEX(size - 1, "update_best_path copy");

        path_state.path_total_cost[PATH_BEST] = cost_current;
        path_state.path_size[PATH_BEST] = size_current;
        path_state.path_total_freq[PATH_BEST] = freq_current;

        memcpy(path_state.path_stack[PATH_BEST], path_state.path_stack[PATH_CURRENT], size * sizeof(uint32_t));
        memcpy(path_state.cost_stack[PATH_BEST], path_state.cost_stack[PATH_CURRENT], size * sizeof(int32_t));
        memcpy(path_state.path_freqs[PATH_BEST], path_state.path_freqs[PATH_CURRENT], size * sizeof(uint32_t));
        memcpy(path_state.path_per_node_costs[PATH_BEST], path_state.path_per_node_costs[PATH_CURRENT],
               size * sizeof(double));

        ret = 1;
    }

    return ret;
}

/**
 * Prints either the current path or the best path.
 * @param isCurrent If true, prints current path; otherwise prints best path
 * @param shouldPrintData If true, prints sequence details as well
 * @param block Pointer to input block (for sequence data)
 */
void print_path(uint8_t isCurrent, uint8_t shouldPrintData, const uint8_t *block) {
    const int idx = isCurrent ? PATH_CURRENT : PATH_BEST;
    const int32_t size = path_state.path_size[idx];
    CHECK_INDEX(size, "print_path");

    const double total_cost = path_state.path_total_cost[idx];
    const uint32_t *stack = path_state.path_stack[idx];
    const uint32_t *freqs = path_state.path_freqs[idx];
    const double *per_node_costs = path_state.path_per_node_costs[idx];

    printf("\n=== %s PATH ===\n", isCurrent ? "CURRENT" : "BEST");
    printf("Path size = %d, Total cost = %.2lf, Total freq=%u \n", size + 1, total_cost,
           path_state.path_total_freq[idx]);
    printf("Node chain (node_id, level):\n");

    for (int32_t i = size; i >= 0; i--) {
        GraphNode *node = get_graph_node(stack[i]);
        if (!node) continue;
        printf("(%u,%u)", node->node_id, node->node_level);
        if (i > 0) printf(" -> ");
    }
    printf("\n");

    if (!shouldPrintData) return;

    printf("\nDetailed sequence info:\n");
    for (int32_t i = size; i >= 0; i--) {
        GraphNode *node = get_graph_node(stack[i]);
        if (!node) continue;

        const uint8_t len = node->sequence_length;
        const uint32_t freq = freqs[i];
        const double cost = per_node_costs[i];

        printf(" -> ");
        if (node->is_RLE) {
            printf("RLE=YES ");
        }

        printf("| id=%u len=%u freq=%u cost=%.2f | ", node->node_id, len, freq, cost);

        print_node_sequence(node, block);

        if (i % 20 == 0) fflush(stdout);
    }

    printf("\n\n");
}

/**
 * Handles backtracking by removing the node from current path,
 * decreasing its frequency if needed, and updating costs.
 */
static inline void backtrack_node(uint32_t node_id, const uint8_t *block) {
    GraphNode *node = get_graph_node(node_id);

    int32_t index = path_state.path_size[PATH_CURRENT];
    CHECK_INDEX(index, "backtrack_node");

    path_state.path_total_cost[PATH_CURRENT] -= path_state.cost_stack[PATH_CURRENT][index];

    if (node->sequence_length > 1 && !node->is_RLE) {
        uint32_t new_freq = node->frequency;
        path_state.path_total_freq[PATH_CURRENT] -= new_freq;
#ifdef DEBUG
        printf("DECR: node_id=%u, new_freq=%u, seq=", node->node_id, new_freq);
        print_node_sequence(node, block);
#endif
    }

    path_state.path_size[PATH_CURRENT]--;
#ifdef DEBUG
    printf("backtrack node %u, stack_size=%u\n", node->node_id, path_state.path_size[PATH_CURRENT]);
#endif
}

/**
 * Processes a node by adding it to the current path, updating frequencies,
 * and calculating its cost contribution.
 */
static inline void process_node(const uint8_t *block, GraphNode *node) {

    int32_t index = ++path_state.path_size[PATH_CURRENT];
#ifdef DEBUG
    printf("Push node %u, stack_size=%u, seq=\n", node->node_id, index);
    print_node_sequence(node, block);
#endif
    CHECK_INDEX(index, "process_node");
    path_state.path_stack[PATH_CURRENT][index] = node->node_id;

    uint32_t freq = 0;
    if (node->sequence_length > 1 && !node->is_RLE) {
        freq = node->frequency;
        // printf("\n node_id=%u, freq=%d \n", node->node_id, freq);
    }

    double added_cost = calc_savings(node, freq);
    if (node->node_id == 0) added_cost = 0;
    path_state.cost_stack[PATH_CURRENT][index] = added_cost;
    path_state.path_total_cost[PATH_CURRENT] += added_cost;
    path_state.path_freqs[PATH_CURRENT][index] = freq;
    path_state.path_per_node_costs[PATH_CURRENT][index] = added_cost;
    path_state.path_total_freq[PATH_CURRENT] += freq;
}

static void decrement_freq(uint16_t last_level, const uint8_t *block) {
    GraphNode *current_node;
    for (uint32_t id = 0; id < get_graph_size(); id++) {
        current_node = get_graph_node(id);
        if (current_node->sequence_length > 1 && !current_node->is_RLE) {
            uint32_t freq, old_node_id;
            uint32_t index = seq_freq_get_with_index(&block[current_node->offset], current_node->sequence_length, &freq,
                                                     &old_node_id);
            if (index != UINT32_MAX) {
                continue;
            }
            if (old_node_id > current_node->node_id) {
                seq_freq_set_existing(index, freq - 1, current_node->node_id);
            } else {
                GraphNode *old_node = get_graph_node(old_node_id); // get the old node.
                if (old_node->node_level <= get_parent_level(current_node)) {
                    seq_freq_set_existing(index, freq - 1, current_node->node_id);
                }
            }
        }

        if (current_node->node_level > last_level) {
            break;
        }
    }
}

static void bookkeeping_best_path(uint16_t last_level, const uint8_t *block) {
    // Step 1. Mark all nodes useless.
    GraphNode *node;
    for (uint32_t id = 0; id < get_graph_size(); id++) {
        node = get_graph_node(id);
        if (node->node_level > last_level) {
            break;
        }
        node->useless = 1; // it is useless.
    }
    // only nodes in the best path are marked useful
    for (uint32_t i = 0; i <= path_state.path_size[PATH_BEST]; i++) {
        node = get_graph_node(path_state.path_stack[PATH_BEST][i]);
#ifdef DEBUG
        printf("\n Marking useful %u\n", node->node_id);
#endif
        node->useless = 0;
        if (node->sequence_length > 1 && !node->is_RLE) {
            seq_freq_increment(&block[node->offset], node->sequence_length,
                               node->node_id); // increment their frequencies.
        }
    }
}
/**
 * Initializes the DFS stack with all valid leaf nodes.
 */
static inline void initialize_leaf_nodes(StackItem *stack, int *top, uint16_t last_level) {
    uint32_t start = get_level_start_id(last_level);
    uint32_t end = get_level_end_id(last_level);
    for (uint32_t i = start; i < end; i++) {
        GraphNode *node = get_graph_node(i);
        if (node) {
            stack[++(*top)] = (StackItem){.node_id = i, .node_id_popped = 0};
        }
    }
}

static inline uint8_t should_prune(GraphNode *node) {
    if (node->useless) return 1;
    /*if (path_state.path_total_cost[PATH_CURRENT] + node->min_depth > path_state.path_total_cost[PATH_BEST] ||
        (path_state.path_total_cost[PATH_CURRENT] + node->min_depth == path_state.path_total_cost[PATH_BEST] &&
         path_state.path_size[PATH_CURRENT] + node->min_depth > path_state.path_size[PATH_BEST])) {
            prune_count++;
            return 1;
    }*/
    return 0;
}

/**
 * Adds all valid parent nodes to the DFS stack for exploration.
 */
static inline void add_parent_nodes_to_stack(StackItem *stack, int *top, GraphNode *node, const uint8_t *block) {
    uint8_t parent_count = get_parent_nodes_count(node);
    GraphNode *parents = get_parent_nodes(node);

    for (uint8_t i = 0; i < parent_count; i++) {
        GraphNode *parent = &parents[i];
        if (should_prune(
                parent) /*&& (node->node_level < get_last_level_index()-1 && parent->min_depth == node->min_depth)*/) {
#ifdef DEBUG
            printf("Prune by cost: parent node_id=%u\n", parent->node_id);
#endif
            continue;
        }
        // Passed all pruning checks, push to stack
        stack[++(*top)] = (StackItem){.node_id = parent->node_id, .node_id_popped = 0};
        if (*top < 0 || *top >= MIN(total_input_size * 2 + 1, BLOCK_SIZE * 2 + 1)) {
            fprintf(stderr, "ERROR: DFS stack top %d out of bounds [0..%ld]\n", *top,
                    MIN(total_input_size * 2 + 1, BLOCK_SIZE * 2 + 1) - 1);
            abort();
        }
    }
}

/**
 * Performs a DFS-based traversal (using a manual stack to avoid recursion)
 * to find the shortest-cost path from any leaf node to the root node (node_id
 * == 0) in a DAG (Directed Acyclic Graph) representing a compression graph.
 *
 * The path cost is computed based on the frequency and length of sequences at
 * each node using calc_cost function.
 */
void find_shortest_path_to_sink(const uint8_t *block, uint16_t starting_level) {

    uint32_t stack_size = TOTAL_GRAPH_NODES(SEQ_LENGTH_LIMIT, starting_level + 1) *
                          2; // note: there is one extra level with no data. Count it!
    StackItem main_stack[stack_size];
    int top = -1;

    uint32_t back_track_count = 0;
    uint32_t best_count = 0;
    uint32_t push_count = 0;
    path_init(); // Reset path state

    initialize_leaf_nodes(main_stack, &top, starting_level);
    decrement_freq(starting_level, block);

    while (top >= 0) {
        StackItem current = main_stack[top--];

        if (current.node_id == UINT32_MAX) {
            // Backtrack marker encountered
            backtrack_node(current.node_id_popped, block);
            back_track_count++;
            continue;
        }
        GraphNode *node = get_graph_node(current.node_id);
        if (node->useless) continue;

        // The following pruning is very useful for speed up.
        // In it, we do not explore paths which are worse.
        // Early pruning before frequency cost and stack updates
        if (should_prune(node)) {
#ifdef DEBUG
            printf("Prune longer path node_id=%u\n", node->node_id);
#endif

            continue;
        }

        process_node(block, node);
        push_count++;

        // Push backtrack marker
        main_stack[++top] = (StackItem){.node_id = UINT32_MAX, .node_id_popped = current.node_id};

        // If reached root node (ID 0), check and update best path
        if (current.node_id == 0) {

#ifdef DEBUG
            print_path(1, 1, block);
#endif
            if (update_best_path()) {
                best_count++;
#ifdef DEBUG
                printf("Saved the path %d with cost: %lf\n", best_count, path_state.path_total_cost[PATH_CURRENT]);
                printf("\nbest_count=%u, prune_count=%u, back_track_count=%u, push_count=%u\n", best_count, prune_count,
                       back_track_count, push_count);
                print_path(0, 1, block);
#endif
                push_count = 0;
                back_track_count = 0;
                prune_count = 0;
            }
            continue; // Root has no parents
        }

        // Explore parents
        add_parent_nodes_to_stack(main_stack, &top, node, block);
    }
    // Final output
#ifdef DEBUG
    printf("\nbest_count=%u, prune_count=%u, back_track_count=%u, push_count=%u\n", best_count, prune_count,
           back_track_count, push_count);
    print_path(0, 1, block);
#endif
    bookkeeping_best_path(starting_level, block);

    // free_path_state();
}

void free_path_state() {
    // Only if path_state has dynamic allocations
    memset(&path_state, 0, sizeof(Path));
}