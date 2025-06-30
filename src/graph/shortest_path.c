// shortest_path.c
#include "shortest_path.h"
#include "graph.h"
#include <stdio.h>
#include <limits.h>
#include "map/sequence_repository_freq.h"

#define MAX_STACK_SIZE MAX_LEVELS

#define COST(len, freq) (((len) == 1) ? 1 : (((freq) == 1) ? ((len) + 1) : 1))


typedef struct {
    uint32_t node_id;
    uint32_t node_id_popped;
} StackItem;

typedef struct {
    uint32_t current_path_stack[MAX_LEVELS];
    int32_t  cost_stack[MAX_LEVELS];  // cost added by each node
    int32_t  current_path_size;
    int32_t  current_path_cost;
    uint32_t best_path_stack[MAX_LEVELS];
    int32_t  best_path_cost;
    int32_t  best_path_size;
} Path;



Path path_state;
void path_init() {
    path_state.current_path_size = -1;
    path_state.best_path_size = -1;
    path_state.current_path_cost = 0;
    path_state.best_path_cost = INT32_MAX;   
}

void reconstruct_path(uint32_t* parent) {
    // Reconstruct shortest path from root (id=0) using parent[]
    printf("\nShortest path to sink (root node 0):\n");
    uint32_t path[MAX_GRAPH_NODES];
    uint32_t length = 0;
    uint32_t current = 0;

    while (current != UINT32_MAX) {
        path[length++] = current;
        current = parent[current];
    }

    for (int i = length - 1; i >= 0; i--) {
        printf("%u%s", path[i], (i > 0 ? " -> " : "\n"));
    }
}


static inline void update_best_path() {
    if (path_state.current_path_cost < path_state.best_path_cost) {
        path_state.best_path_cost = path_state.current_path_cost;
        memcpy(path_state.best_path_stack,
               path_state.current_path_stack,
               (path_state.current_path_size + 1) * sizeof(uint32_t)); // include top
        path_state.best_path_size = path_state.current_path_size;
    }
}


static inline void print_current_path() {
    for (int32_t stack_index = 0; stack_index <= path_state.current_path_size; stack_index++) {
        printf("%u", path_state.current_path_stack[stack_index]);
        if (stack_index + 1 <= path_state.current_path_size) {
            printf(" -> ");
        }
        fflush(stdout);
    }
    printf("\n");
}


/**
 * Performs a DFS-based traversal (using a manual stack to avoid recursion) 
 * to find the shortest-cost path from any leaf node to the root node (node_id == 0)
 * in a DAG (Directed Acyclic Graph) representing a compression graph.
 *
 * The path cost is computed based on the frequency and length of sequences at each node:
 * - If sequence_length == 1: cost = 1
 * - If sequence_length > 1 and frequency == 1: cost = sequence_length + 1
 * - If sequence_length > 1 and frequency > 1: cost = 1
 *
 * The frequency is updated during the forward traversal and rolled back when backtracking.
 * The best (lowest-cost) path to the root is stored and printed at the end.
 */
void find_shortest_path_to_sink(const uint8_t *block) {
    uint16_t last_level = get_last_level_index();
    uint32_t start = get_level_start_id(last_level);
    uint32_t end = get_level_end_id(last_level);

    StackItem main_stack[MAX_STACK_SIZE];
    int top = -1;

    path_init();      // Reset path state
    seq_repo_reset(); // Reset sequence frequencies (memory reused)

    static int32_t min_cost_to_node[MAX_GRAPH_NODES];
    for (uint32_t i = 0; i < MAX_GRAPH_NODES; i++) {
        min_cost_to_node[i] = INT32_MAX;
    }

    // Start DFS from all non-useless leaf nodes
    for (uint32_t i = start; i < end; i++) {
        GraphNode *node = get_graph_node(i);
        if (node && !node->isUseless) {
            main_stack[++top] = (StackItem){.node_id = i, .node_id_popped = 0};
        }
    }

    while (top >= 0) {
        StackItem current = main_stack[top--];

        if (current.node_id == UINT32_MAX) {
            // Backtrack: pop node and undo frequency and cost
            GraphNode *node = get_graph_node(current.node_id_popped);
            uint32_t seq_len = node->sequence_length;

            path_state.current_path_cost -=
                path_state.cost_stack[path_state.current_path_size];
            seq_repo_decrease_frequency(&block[node->offset], seq_len);
            path_state.current_path_size--;
            continue;
        }

        GraphNode *node = get_graph_node(current.node_id);
        uint32_t node_id = node->node_id;

        // Push node to current path
        path_state.current_path_stack[++path_state.current_path_size] = node_id;

        // If reached root node (ID 0), check and update best path
        if (node_id == 0) {
            printf(" saved the path with cost: %d\n",
                   path_state.current_path_cost);
#ifdef DEBUG
            print_current_path();
#endif
            update_best_path();
            // Don't push parents; root has none
            continue;
        }

        // Increase frequency and compute added cost
        uint32_t freq = seq_repo_increase_frequency(&block[node->offset],
                                                    node->sequence_length);
        int32_t added_cost = COST(node->sequence_length, freq);

        // Skip expanding if we’ve already reached this node at lower cost
        if (path_state.current_path_cost >= min_cost_to_node[node_id]) {
            // Still push backtrack marker to correctly pop
            main_stack[++top] =
                (StackItem){.node_id = UINT32_MAX, .node_id_popped = node_id};
            continue;
        }

        // Record better cost for this node
        min_cost_to_node[node_id] = path_state.current_path_cost;

        path_state.cost_stack[path_state.current_path_size] = added_cost;
        path_state.current_path_cost += added_cost;

        // If path is already worse than best, backtrack immediately
        if (path_state.current_path_cost >= path_state.best_path_cost) {
            main_stack[++top] =
                (StackItem){.node_id = UINT32_MAX, .node_id_popped = node_id};
            continue;
        }

        // Push backtrack marker
        main_stack[++top] =
            (StackItem){.node_id = UINT32_MAX, .node_id_popped = node_id};

        // Explore parents
        uint8_t parent_count = get_parent_nodes_count(node);
        GraphNode *parents = get_parent_nodes(node);
        for (uint8_t i = 0; i < parent_count; i++) {
            GraphNode *parent = &parents[i];
            if (parent->isUseless)
                continue;

            // Estimate next cost: current + hypothetical cost of parent
            uint32_t est_freq = seq_repo_get_frequency(&block[parent->offset],
                                                       parent->sequence_length);
            int32_t estimated_cost =
                path_state.current_path_cost +
                COST(parent->sequence_length, est_freq + 1);

            // Skip if even the next step would already exceed best cost
            if (estimated_cost >= path_state.best_path_cost)
                continue;

            // Skip if this parent was already visited with better or equal cost
            if (estimated_cost >= min_cost_to_node[parent->node_id])
                continue;

            main_stack[++top] =
                (StackItem){.node_id = parent->node_id, .node_id_popped = 1};
        }
    }

    // Final output
    printf("\nShortest path size=%d, cost: %d\nPath: ",path_state.best_path_size, path_state.best_path_cost);
#ifdef DEBUG
    for (int i = path_state.best_path_size; i >= 0; i--) {     
        printf("%u%s", path_state.best_path_stack[i],
               (i > 0 ? " -> " : "\n"));
    }
    for (int i = path_state.best_path_size; i > 0; i--) {
        uint32_t node_id = path_state.best_path_stack[i];
        GraphNode* node = get_graph_node(node_id);
        print_node_sequence(node, block);
        printf("%s", (i > 0 ? " -> " : "\n"));
    }
#endif
}
