// shortest_path.c

#include "shortest_path_common.h"
#include <stdbool.h>
#include <math.h>

int prune_count = 0;

typedef struct {
    uint32_t node_id;
    uint32_t node_id_popped;
    uint32_t hash_index_cache;
} StackItem;

Path path_state;
static StackItem main_stack[MAX_GRAPH_NODES*2+1];



/**
 * Handles backtracking by removing the node from current path,
 * decreasing its frequency if needed, and updating savings.
 */
static inline void backtrack_node() {

    int32_t index = path_state.path_size[PATH_CURRENT];
    CHECK_INDEX(index, "backtrack_node");

    uint32_t freq = path_state.path_freqs[PATH_CURRENT][index];
    path_state.path_total_freq[PATH_CURRENT] -= freq;
    path_state.path_total_saving[PATH_CURRENT] -= path_state.path_per_node_savings[PATH_CURRENT][index];
    path_state.path_stack[PATH_CURRENT][index] = 0;
    path_state.path_per_node_savings[PATH_CURRENT][index] = 0;
    path_state.path_freqs[PATH_CURRENT][index] = 0;
    path_state.path_size[PATH_CURRENT]--;
}

/**
 * Processes a node by adding it to the current path, updating frequencies,
 * and calculating its saving contribution.
 */
static inline void process_node(const uint8_t *block, GraphNode *node) {

    int32_t index = ++path_state.path_size[PATH_CURRENT];
#ifdef DEBUG
    printf("Add in path node %u, stack_size=%u, seq=", node->node_id, index);
    print_node_sequence(node, block);
    printf("\n");
#endif
    CHECK_INDEX(index, "process_node");
    path_state.path_stack[PATH_CURRENT][index] = node->node_id;

    uint32_t freq = 0, node_id;
    if (node->sequence_length > 1 && !node->is_RLE) {
        seq_freq_get(&block[node->offset], node->sequence_length, &freq, &node_id);
    }

    double added_saving = calc_savings(node, freq);
    if (node->node_id == 0) added_saving = 0;
    path_state.path_total_saving[PATH_CURRENT] += added_saving;
    path_state.path_freqs[PATH_CURRENT][index] = freq;
    path_state.path_per_node_savings[PATH_CURRENT][index] = added_saving;
    path_state.path_total_freq[PATH_CURRENT] += freq;
}

static void bookkeeping_best_path(uint16_t last_level, const uint8_t *block) {
    // Step 1. Mark all nodes useless.
    GraphNode *node;
    uint32_t start_id = get_level_start_id(last_level - MAX_BRUTE_FORCE_PATH);
    uint32_t end_id = get_level_end_id(last_level);
    for (uint32_t id = start_id; id < end_id; id++) {
        node = get_graph_node(id);
        if (!node->useless) {
            node->useless = 1; // mark it useless.
            if (node->sequence_length > 1 && !node->is_RLE) {
                uint32_t freq, node_id;
                uint32_t index = seq_freq_get_with_index(&block[node->offset], node->sequence_length, &freq, &node_id);
                if (freq > 1) {
                    seq_freq_set_existing(index, freq - 1, node_id);
                }
            }
        }
    }
    // only nodes in the best path are marked useful
    for (uint32_t i = 0; i <= path_state.path_size[PATH_BEST]; i++) {
        uint32_t node_id = path_state.path_stack[PATH_BEST][i];
        if (node_id < start_id) continue;
        node = get_graph_node(node_id);
#ifdef DEBUG
        printf("\n Marking useful %u\n", node_id);
#endif
        node->useless = 0;
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
        if (node->is_RLE) {
            printf("Leaf level has RLE node\n");
        }
        if (node) {
            stack[++(*top)] = (StackItem){.node_id = i, .node_id_popped = 0};
#ifdef DEBUG
            printf("Leafs: at stack position %d, we put the node %u\n", *top, node->node_id);
            print_graph_node(node);
#endif
        }
    }
}

static inline uint8_t should_prune(GraphNode *node) {
    if (node->useless) return 1;
    /*if (path_state.path_total_saving[PATH_CURRENT] + node->min_depth > path_state.path_total_saving[PATH_BEST] ||
        (path_state.path_total_saving[PATH_CURRENT] + node->min_depth == path_state.path_total_saving[PATH_BEST] &&
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
            printf("Prune by saving: parent node_id=%u\n", parent->node_id);
#endif
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

/**
 * Performs a DFS-based traversal (using a manual stack to avoid recursion)
 * to find the best-saving path from any leaf node to the root node (node_id
 * == 0) in a DAG (Directed Acyclic Graph) representing a compression graph.
 *
 * The path saving is computed based on the frequency and length of sequences at
 * each node using calc_saving function.
 */
void find_best_saving_path(const uint8_t *block, uint16_t starting_level) {
  
    int top = -1;

    uint32_t back_track_count = 0;
    uint32_t best_count = 0;
    uint32_t push_count = 0;

    path_init(); // Reset path state

    initialize_leaf_nodes(main_stack, &top, starting_level);
    if(top < 0) {
        printf("empty level\n");
        return; //no path exist.
    }
    while (top >= 0) {
        StackItem current = main_stack[top--];
#ifdef DEBUG
        printf("pop stack node_id=%u, from top=%d, node_id_popped=%u\n", current.node_id, top + 1,
               current.node_id_popped);
#endif

        if (current.node_id == UINT32_MAX) {
            // Backtrack marker encountered
            backtrack_node();
            back_track_count++;
            continue;
        }
        GraphNode *node = get_graph_node(current.node_id);
        if (node->useless) continue;

        // The following pruning is very useful for speed up.
        // In it, we do not explore paths which are worse.
        // Early pruning before frequency saving and stack updates
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
#ifdef DEBUG
        printf("Push in stack position %d backrack for node=%u\n", top, node->node_id);
#endif

        // If reached root node (ID 0), check and update best path
        if (current.node_id == 0) {

#ifdef DEBUG
            print_path(1, 1, block);
#endif
            if (update_best_path()) {
                best_count++;
#ifdef DEBUG
                printf("Saved the path %d with saving: %lf\n", best_count, path_state.path_total_saving[PATH_CURRENT]);
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

