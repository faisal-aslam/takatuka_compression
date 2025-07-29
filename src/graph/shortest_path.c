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
 * @brief Calculates the storage saving
 *
 * @param node Pointer to graph node being evaluated
 * @param frequency Frequency count of this sequence in the data
 * @return double Storage saving in bytes
 */
static inline double calc_savings(GraphNode *node, uint32_t frequency) {

    // Branchless design for common cases - reduces pipeline stalls
    const uint8_t len = node->sequence_length;
    double base_saving;

    if (node->node_id == 0) return 0; // no savings for the root node.

    // Handle RLE case first (uses different saving model)
    if (node->is_RLE) {
        // RLE saving: pattern length + 1 byte for repeat count
        double ret = (node->length_of_RLE / node->repeat_seq_length);
        return (ret * ret);
    }

    // Main saving calculation branches
    if (len <= 1) {
        // Cases: 0 bytes = 0/1 saving, 0
        base_saving = 0;
    } else {
        // Multi-byte case: savings is based on length and frequency.
        base_saving = frequency * node->sequence_length * node->sequence_length;
    }

    return base_saving;
}

/**
 * Initializes the path state for a new search.
 */
static inline void path_init() {
    memset(&path_state, 0, sizeof(Path));
    path_state.path_size[PATH_CURRENT] = -1;
    path_state.path_size[PATH_BEST] = -1;
    path_state.path_total_saving[PATH_CURRENT] = 0;
    path_state.path_total_saving[PATH_BEST] = -1;
    path_state.path_total_freq[PATH_BEST] = 0;
    path_state.path_total_freq[PATH_CURRENT] = 0;
}

/**
 * Updates the best path if the current path is better.
 */
static inline uint8_t update_best_path() {
    uint8_t ret = 0;

    int32_t saving_current = path_state.path_total_saving[PATH_CURRENT];
    int32_t saving_best = path_state.path_total_saving[PATH_BEST];
    int32_t size_current = path_state.path_size[PATH_CURRENT];
    int32_t size_best = path_state.path_size[PATH_BEST];
    uint32_t freq_current = path_state.path_total_freq[PATH_CURRENT];
    uint32_t freq_best = path_state.path_total_freq[PATH_BEST];

    if (saving_current > saving_best || (saving_current == saving_best && size_current < size_best) ||
        (saving_current == saving_best && size_current == size_best && freq_current > freq_best)) {

        int32_t size = size_current + 1;
        CHECK_INDEX(size - 1, "update_best_path copy");

        path_state.path_total_saving[PATH_BEST] = saving_current;
        path_state.path_size[PATH_BEST] = size_current;
        path_state.path_total_freq[PATH_BEST] = freq_current;

        memcpy(path_state.path_stack[PATH_BEST], path_state.path_stack[PATH_CURRENT], size * sizeof(uint32_t));
        memcpy(path_state.path_freqs[PATH_BEST], path_state.path_freqs[PATH_CURRENT], size * sizeof(uint32_t));
        memcpy(path_state.path_per_node_savings[PATH_BEST], path_state.path_per_node_savings[PATH_CURRENT],
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
    CHECK_INDEX(size - 1, "print_path");

    const double total_saving = path_state.path_total_saving[idx];
    const uint32_t *stack = path_state.path_stack[idx];
    const uint32_t *freqs = path_state.path_freqs[idx];
    const double *per_node_savings = path_state.path_per_node_savings[idx];

    printf("\n=== %s PATH ===\n", isCurrent ? "CURRENT" : "BEST");
    printf("Path size = %d, Total saving = %.2lf, Total freq=%u \n", size + 1, total_saving,
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
        const double saving = per_node_savings[i];

        printf("\n -> ");
        if (node->is_RLE) {
            printf("RLE=YES ");
        }

        printf("| id=%u len=%u freq=%u saving=%.2f | ", node->node_id, len, freq, saving);

        print_node_sequence(node, block);

        if (i % 20 == 0) fflush(stdout);
    }

    printf("\n\n");
}

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
    for (uint32_t id = 0; id < get_graph_size(); id++) {
        node = get_graph_node(id);
        if (node->node_level > last_level) {
            break;
        }
        node->useless = 1; // it is useless.
        if (node->sequence_length > 1 && !node->is_RLE) {
            uint32_t freq, node_id;
            uint32_t index = seq_freq_get_with_index(&block[node->offset], node->sequence_length, &freq, &node_id);
            if (index != UINT32_MAX && freq > 0) {
                seq_freq_set_existing(index, freq - 1, node->node_id);
            }
        }
    }
    // only nodes in the best path are marked useful
    for (uint32_t i = 0; i <= path_state.path_size[PATH_BEST]; i++) {
        node = get_graph_node(path_state.path_stack[PATH_BEST][i]);
#ifdef DEBUG
        printf("\n Marking useful %u\n", node->node_id);
#endif
        node->useless = 0;
        if (node->sequence_length > 1 && !node->is_RLE) {
            uint32_t freq, node_id;
            uint32_t index = seq_freq_get_with_index(&block[node->offset], node->sequence_length, &freq, &node_id);
            if (index != UINT32_MAX) {
                seq_freq_set_existing(index, freq + 1, node->node_id);
            }
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

    // note: there is one extra level with no data. Count it!
    uint32_t stack_size = TOTAL_GRAPH_NODES(SEQ_LENGTH_LIMIT+1, starting_level + 1) * 2; 
    
    StackItem main_stack[stack_size];
    int top = -1;

    uint32_t back_track_count = 0;
    uint32_t best_count = 0;
    uint32_t push_count = 0;

    path_init(); // Reset path state

    initialize_leaf_nodes(main_stack, &top, starting_level);
    while (top >= 0) {
        StackItem current = main_stack[top--];
#ifdef DEBUG
        printf("pop stack node_id=%u, from top=%d, node_id_popped=%u\n", current.node_id, top+1, current.node_id_popped);
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

void final_book_keeping(const uint8_t* block) {
    init_seq_freq_map();
    GraphNode *node;
    const uint32_t* path = path_state.path_stack[PATH_BEST];
    uint32_t path_len = path_state.path_size[PATH_BEST];

    // Pass 1: Count sequence frequencies
    for (uint32_t i = 0; i <= path_len; i++) {
        node = get_graph_node(path[i]);
        if (node->sequence_length > 1 && !node->is_RLE) {
            seq_freq_increment(&block[node->offset], node->sequence_length, node->node_id);
        }
    }

    // Pass 2: Store frequencies per node
    for (uint32_t i = 0; i <= path_len; i++) {
        node = get_graph_node(path[i]);
        if (node->sequence_length > 1 && !node->is_RLE) {
            uint32_t freq, node_id_unused;
            seq_freq_get(&block[node->offset], node->sequence_length, &freq, &node_id_unused);
            path_state.path_freqs[PATH_BEST][i] = freq;
        } else {
            path_state.path_freqs[PATH_BEST][i] = 1; // or other sentinel if needed
        }
    }
}


void free_path_state() {
    // Only if path_state has dynamic allocations
    memset(&path_state, 0, sizeof(Path));
}