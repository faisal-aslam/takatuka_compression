// shortest_path.c

#include "shortest_path_common.h"
#include <math.h>
#include <stdbool.h>

typedef struct {
    uint32_t node_id;
    uint32_t node_id_popped;
    uint32_t hash_index_cache;
} StackItem;

static uint64_t push_count;
/**
 * Handles backtracking by removing the node from current path,
 * decreasing its frequency if needed, and updating savings.
 */
static inline void backtrack_node(const uint8_t *block, Path *path_state) {

    int32_t index = path_state->path_size[PATH_CURRENT];
    CHECK_INDEX(index, "backtrack_node");

    uint32_t node_id = path_state->path_stack[PATH_CURRENT][index];
    GraphNode *node = get_graph_node(node_id);
    if (!node->is_RLE && node->sequence_length > 1) {

        seq_freq_decrement(&block[node->offset], node->sequence_length);
    }
    path_state->path_total_freq[PATH_CURRENT] -= path_state->path_freqs[PATH_CURRENT][index];
    path_state->path_total_saving[PATH_CURRENT] -= path_state->path_per_node_savings[PATH_CURRENT][index];
    path_state->path_total_cost[PATH_CURRENT] -= path_state->path_per_node_cost[PATH_CURRENT][index];
    path_state->path_stack[PATH_CURRENT][index] = 0;
    path_state->path_per_node_savings[PATH_CURRENT][index] = 0;
    path_state->path_per_node_cost[PATH_CURRENT][index] = 0; // backtrack cost.
    path_state->path_freqs[PATH_CURRENT][index] = 0;
    path_state->path_size[PATH_CURRENT]--;
#ifdef DEBUG
    printf("After backtrack node=%u, Cost=%u, savings=%u, size=%u\n", node_id,
           path_state->path_total_cost[PATH_CURRENT], path_state->path_total_saving[PATH_CURRENT],
           path_state->path_size[PATH_CURRENT]);
    seq_freq_map_print();
#endif
}

/**
 * Processes a node by adding it to the current path, updating frequencies,
 * and calculating its saving contribution.
 */
static inline void process_node(const uint8_t *block, GraphNode *node, Path *path_state) {
    push_count++;
    int32_t index = ++path_state->path_size[PATH_CURRENT];
#ifdef DEBUG
    printf("Add in path node %u, stack_size=%u, seq=", node->node_id, index);
    print_node_sequence(node, block);
    printf("\n");
#endif
    CHECK_INDEX(index, "process_node");
    path_state->path_stack[PATH_CURRENT][index] = node->node_id;

    uint32_t freq = 0;

    if (node->sequence_length > 1 && !node->is_RLE) {
        freq = seq_freq_increment(&block[node->offset], node->sequence_length, node->node_id);
    }
#ifdef DEBUG
    seq_freq_map_print();
#endif
    uint32_t added_saving = calc_savings(node, freq);
    uint32_t added_cost = calc_cost(node, freq);
    if (node->node_id == 0) added_saving = 0;
    path_state->path_total_saving[PATH_CURRENT] += added_saving;
    path_state->path_freqs[PATH_CURRENT][index] = freq;
    path_state->path_per_node_savings[PATH_CURRENT][index] = added_saving;
    path_state->path_total_freq[PATH_CURRENT] += freq;
    path_state->path_total_cost[PATH_CURRENT] += added_cost;
    path_state->path_per_node_cost[PATH_CURRENT][index] = added_cost;
#ifdef DEBUG
    printf("So far, Cost=%u, savings=%u, size=%u\n", path_state->path_total_cost[PATH_CURRENT],
           path_state->path_total_saving[PATH_CURRENT], path_state->path_size[PATH_CURRENT]);
#endif
}

static void bookkeeping_best_path(const uint8_t *block, Path *path_state) {

    GraphNode *node;

    for (int32_t i = 0; i <= path_state->path_size[PATH_BEST]; i++) {
        uint32_t node_id = path_state->path_stack[PATH_BEST][i];
        node = get_graph_node(node_id);
        if (node->sequence_length > 1 && !node->is_RLE) {
            seq_freq_increment(&block[node->offset], node->sequence_length, node->node_id);
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
        if (node->useless) continue;
        if (node) {
            stack[++(*top)] = (StackItem){.node_id = i, .node_id_popped = 0};
#ifdef DEBUG
            printf("Leafs: at stack position %d, we put the node %u\n", *top, node->node_id);
            print_graph_node(node);
#endif
        }
    }
}

static inline uint8_t should_prune(GraphNode *node, GraphNode *dest_node, Path *path_state) {
    if (node->useless || node->node_level < dest_node->node_level) return 1;
    if (path_state->path_total_cost[PATH_CURRENT] > path_state->path_total_cost[PATH_BEST] ||
        (path_state->path_total_cost[PATH_CURRENT] == path_state->path_total_cost[PATH_BEST] &&
         path_state->path_total_saving[PATH_CURRENT] < path_state->path_total_saving[PATH_BEST])) {
        return 1;
    }
    return 0;
}

/**
 * Adds all valid parent nodes to the DFS stack for exploration.
 */
static inline void add_parent_nodes_to_stack(StackItem *stack, int *top, GraphNode *node, const uint8_t *block,
                                             GraphNode *dest_node, Path *path_state) {
    (void)block; // Mark as intentionally unused
    if (get_parent_level(node) == dest_node->node_level) {
        stack[++(*top)] = (StackItem){.node_id = dest_node->node_id, .node_id_popped = 0};
        return;
    }
    uint8_t parent_count = get_parent_nodes_count(node);
    GraphNode *parents = get_parent_nodes(node);
    for (uint8_t i = 0; i < parent_count; i++) {
        GraphNode *parent = &parents[i];
        if (should_prune(parent, dest_node, path_state)) {
            continue;
        }
        // Passed all pruning checks, push to stack
        stack[++(*top)] = (StackItem){.node_id = parent->node_id, .node_id_popped = 0};
#ifdef DEBUG
        printf(" Added parent at %i \n", *top);
        print_graph_node(parent);
#endif
        if (*top < 0 || *top >= MIN(total_input_size * 2 + 1, BLOCK_SIZE * 2 + 1)) {
            fprintf(stderr, "ERROR: DFS stack top %d out of bounds [0..%ld]\n", *top,
                    MIN(total_input_size * 2 + 1, BLOCK_SIZE * 2 + 1) - 1);
            abort();
        }
    }
}

// Rounds up to the next power of two for 32-bit numbers.
// Returns 1 if x is 0 (edge case).
static inline uint32_t next_power_of_two(uint32_t x) {
    if (x == 0) return 1;
    x--; // handle exact power of two case
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}

void find_best_saving_path_to_a_node(const uint8_t *block, uint16_t starting_level, uint32_t destination_id,
                                     Path *path_state) {
    int top = -1;
    best_count = 0;
    push_count = 0;
    path_init(path_state); // Reset path state
    GraphNode *dest_node = get_graph_node(destination_id);
    if (dest_node->node_level > starting_level) {
        printf("destination node %u is not reachable from the given level %u\n", dest_node->node_id, starting_level);
        return;
    }
    uint32_t max_nodes = (starting_level - dest_node->node_level) * SEQ_LENGTH_LIMIT * 2;
    max_nodes = next_power_of_two(max_nodes);
    uint64_t max_push = 1000 * max_nodes;
    StackItem *main_stack = malloc(sizeof(StackItem) * max_nodes);
    if (!main_stack) {
        fprintf(stderr, "Error: failed to allocate memory for main_stack\n");
        exit(1); // or handle gracefully
    }
    initialize_leaf_nodes(main_stack, &top, starting_level);
    if (top < 0) {
        printf("empty level\n");
        return; // no path exist.
    }
    while (top >= 0) {
        if ((best_count > 7 && push_count > max_push)|| best_count > 1 && push_count > max_nodes*10000) {
            printf("Max push=%lu, push_count=%lu, best_count=%u\n", max_push, push_count, best_count);
            break; // we are done trying.
        }
        
        StackItem current = main_stack[top--];
#ifdef DEBUG
        printf("pop stack node_id=%u, from top=%d, node_id_popped=%u\n", current.node_id, top + 1,
               current.node_id_popped);
#endif

        if (current.node_id == UINT32_MAX) {
            // Backtrack marker encountered
            backtrack_node(block, path_state);
            continue;
        }
        GraphNode *node = get_graph_node(current.node_id);

        // The following pruning is very useful for speed up.
        // In it, we do not explore paths which are worse.
        // Early pruning before frequency saving and stack updates
        if (should_prune(node, dest_node, path_state)) {
            continue;
        }
        process_node(block, node, path_state);

        // Push backtrack marker
        main_stack[++top] = (StackItem){.node_id = UINT32_MAX, .node_id_popped = current.node_id};
#ifdef DEBUG
        printf("Push in stack position %d backrack for node=%u\n", top, node->node_id);
#endif

        // If reached destination node, check and update best path
        if (current.node_id == destination_id) {

#ifdef DEBUG
            print_path(1, 1, block, path_state);
#endif
            if (update_best_path(block, path_state)) {
                push_count = 0;
#ifdef DEBUG
                printf("Saved the path %d with saving: %u\n", best_count, path_state->path_total_saving[PATH_CURRENT]);
                print_path(0, 1, block, path_state);
#endif
            }
            continue; // Root has no parents
        }

        // explore parents.
        if (get_parent_level(node) >= dest_node->node_level) {
            add_parent_nodes_to_stack(main_stack, &top, node, block, dest_node, path_state);
        }
    }
    // Final output
#ifdef DEBUG
    printf("\nbest_count=%u, \n", best_count);
    print_path(0, 1, block, path_state);
#endif
    bookkeeping_best_path(block, path_state);

    free(main_stack);

    // free_path_state();
}
