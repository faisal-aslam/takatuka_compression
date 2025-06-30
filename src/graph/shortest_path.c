// shortest_path.c

#include "shortest_path.h"
#include "graph.h"
#include "map/sequence_repository_freq.h"
#include <limits.h>
#include <stdio.h>

#define MAX_STACK_SIZE MAX_LEVELS


/**
 * Calculates the cost of adding a sequence to the path based on its length and frequency.
 * Cost rules:
 * - If length == 1: cost = 1
 * - If length > 1 and frequency == 1: cost = length + 1
 * - If length > 1 and frequency > 1: cost = 1
 */
#define COST(len, freq) (((len) == 1) ? 1 : (((freq) == 1) ? ((len) + 1) : 1))


typedef struct {
    uint32_t node_id;
    uint32_t node_id_popped;
} StackItem;

typedef struct {
    uint32_t current_path_stack[MAX_LEVELS];
    int32_t cost_stack[MAX_LEVELS]; // cost added by each node
    int32_t current_path_size;
    int32_t current_path_cost;
    uint32_t best_path_stack[MAX_LEVELS];
    int32_t best_path_cost;
    int32_t best_path_size;
} Path;

Path path_state;

/**
 * Initializes the path state for a new search.
 */
static inline void path_init() {
    path_state.current_path_size = -1;
    path_state.best_path_size = -1;
    path_state.current_path_cost = 0;
    path_state.best_path_cost = INT32_MAX;
}

/**
 * Updates the best path if the current path is better.
 */
static inline void update_best_path() {
    if (path_state.current_path_cost < path_state.best_path_cost) {
        path_state.best_path_cost = path_state.current_path_cost;
        memcpy(path_state.best_path_stack, path_state.current_path_stack,
               (path_state.current_path_size + 1) * sizeof(uint32_t));
        path_state.best_path_size = path_state.current_path_size;
    }
}

/**
 * Prints either the current path or the best path.
 * @param isCurrent If true, prints current path; otherwise prints best path
 */
static void print_path(uint8_t isCurrent, uint8_t shouldPrintData, const uint8_t* block) {
    const int32_t size = isCurrent ? path_state.current_path_size : path_state.best_path_size;
    const int32_t cost = isCurrent ? path_state.current_path_cost : path_state.best_path_cost;
    const uint32_t *stack = isCurrent ? path_state.current_path_stack : path_state.best_path_stack;
    
    printf(" Shorest path size=%d, cost=%d \n", size, cost);
    for (int32_t i = 0; i <= size; i++) {
        printf("%u", stack[i]);
        if (i < size) {
            printf(" -> ");
        }
    }
    printf("\n");
    if (shouldPrintData) {
        for (int32_t i = 0; i < size; i++) {
            uint32_t node_id = stack[i];
            GraphNode *node = get_graph_node(node_id);
            if (!node) continue;
            printf(" -> ");
            print_node_sequence(node, block);
            
        }
    }
}

/**
 * Handles backtracking by removing the node from current path,
 * decreasing its frequency if needed, and updating costs.
 */
static inline void backtrack_node(const uint8_t* block, uint32_t node_id) {
    GraphNode *node = get_graph_node(node_id);
#ifdef DEBUG
    printf("backtrack node %u\n", node->node_id);
#endif
    uint32_t seq_len = node->sequence_length;

    path_state.current_path_cost -= path_state.cost_stack[path_state.current_path_size];
    if (seq_len > 1) {
        seq_repo_decrease_frequency(&block[node->offset], seq_len);
    }
    path_state.current_path_size--;
}

/**
 * Processes a node by adding it to the current path, updating frequencies,
 * and calculating its cost contribution.
 */
static inline void process_node(const uint8_t* block, uint32_t node_id) {
    GraphNode *node = get_graph_node(node_id);
#ifdef DEBUG
    printf("Push node %u\n", node->node_id);
#endif

    // Add node to current path
    path_state.current_path_stack[++path_state.current_path_size] = node_id;

    // Update frequency and calculate cost
    uint32_t freq = 1;
    if (node->sequence_length > 1) {
        freq = seq_repo_increase_frequency(&block[node->offset], node->sequence_length);
    }

    int32_t added_cost = COST(node->sequence_length, freq);
    path_state.cost_stack[path_state.current_path_size] = added_cost;
    path_state.current_path_cost += added_cost;
}

/**
 * Initializes the DFS stack with all valid leaf nodes.
 */
static inline void initialize_leaf_nodes(StackItem* stack, int* top, uint16_t last_level) {
    uint32_t start = get_level_start_id(last_level);
    uint32_t end = get_level_end_id(last_level);

    for (uint32_t i = start; i < end; i++) {
        GraphNode *node = get_graph_node(i);
        if (node && !node->isUseless) {
            stack[++(*top)] = (StackItem){.node_id = i, .node_id_popped = 0};
        }
    }
}

/**
 * Adds all valid parent nodes to the DFS stack for exploration.
 */
static inline void add_parent_nodes_to_stack(StackItem* stack, int* top, GraphNode* node) {
    uint8_t parent_count = get_parent_nodes_count(node);
    GraphNode *parents = get_parent_nodes(node);
    for (uint8_t i = 0; i < parent_count; i++) {
        GraphNode *parent = &parents[i];
        if (parent->isUseless) continue;
        stack[++(*top)] = (StackItem){.node_id = parent->node_id, .node_id_popped = 0};
    }
}

/**
 * Performs a DFS-based traversal (using a manual stack to avoid recursion)
 * to find the shortest-cost path from any leaf node to the root node (node_id
 * == 0) in a DAG (Directed Acyclic Graph) representing a compression graph.
 *
 * The path cost is computed based on the frequency and length of sequences at
 * each node using COST Macro.
 */
void find_shortest_path_to_sink(const uint8_t *block) {
    StackItem main_stack[MAX_STACK_SIZE];
    int top = -1;
    uint16_t last_level = get_last_level_index();

    path_init();      // Reset path state
    seq_repo_reset(); // Reset sequence frequencies (memory reused)
    
    initialize_leaf_nodes(main_stack, &top, last_level);

    while (top >= 0) {
        StackItem current = main_stack[top--];

        if (current.node_id == UINT32_MAX) {
            // Backtrack marker encountered
            backtrack_node(block, current.node_id_popped);
            continue;
        }

        process_node(block, current.node_id);
        
        // Push backtrack marker
        main_stack[++top] = (StackItem){.node_id = UINT32_MAX, .node_id_popped = current.node_id};

        // If reached root node (ID 0), check and update best path
        if (current.node_id == 0) {
            printf(" saved the path with cost: %d\n", path_state.current_path_cost);
#ifdef DEBUG
            print_path(1, 0, NULL);
#endif
            update_best_path();
            continue; // Root has no parents
        }

        // Explore parents
        GraphNode *node = get_graph_node(current.node_id);
        add_parent_nodes_to_stack(main_stack, &top, node);
    }

    // Final output
    print_path(0, 1, block);
}