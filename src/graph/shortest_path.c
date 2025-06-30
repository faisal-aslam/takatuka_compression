// shortest_path.c
#include "shortest_path.h"
#include "graph.h"
#include <stdio.h>
#include <limits.h>
#include "map/sequence_repository_freq.h"

#define MAX_STACK_SIZE MAX_LEVELS

typedef struct {
    uint32_t node_id;
    uint32_t node_id_popped;
} StackItem;

typedef struct {
    uint32_t current_path_stack[MAX_LEVELS];
    int32_t current_path_size;
    int32_t current_path_cost;
    uint32_t best_path_stack[MAX_LEVELS];
    int32_t best_path_cost;
    int32_t best_path_size;
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

    path_init(); // Make sure path state is initialized

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
            // Finished processing child paths of this node, now backtrack
            GraphNode *node = get_graph_node(current.node_id_popped);

            // Compute cost to remove
            uint32_t seq_len = node->sequence_length;
            uint32_t freq = seq_repo_get_frequency(&block[node->offset], seq_len);

            if (seq_len == 1) {
                path_state.current_path_cost -= 1;
            } else if (freq == 1) {
                path_state.current_path_cost -= (seq_len + 1);
            } else {
                path_state.current_path_cost -= 1;
            }

            // Pop from path and decrease frequency
            seq_repo_decrease_frequency(&block[node->offset], seq_len);
            path_state.current_path_size--;
            continue;
        }

        GraphNode *node = get_graph_node(current.node_id);

        // Add node to current path
        path_state.current_path_stack[++path_state.current_path_size] = node->node_id;

        // Increase frequency
        uint32_t freq = seq_repo_increase_frequency(&block[node->offset], node->sequence_length);

        // Compute and add cost
        if (node->sequence_length == 1) {
            path_state.current_path_cost += 1;
        } else if (freq == 1) {
            path_state.current_path_cost += (node->sequence_length + 1);
        } else {
            path_state.current_path_cost += 1;
        }

        // If we've reached the root, evaluate and possibly update best path
        if (node->node_id == 0) {
            printf(" saved the path with cost: %d\n", path_state.current_path_cost);
            print_current_path();
            update_best_path();
            continue; // no parents to explore
        }

        // Add marker for backtracking
        main_stack[++top] = (StackItem){.node_id = UINT32_MAX, .node_id_popped = node->node_id};

        // Push parent nodes to explore next
        uint8_t parent_count = get_parent_nodes_count(node);
        GraphNode *parents = get_parent_nodes(node);
        for (uint8_t i = 0; i < parent_count; i++) {
            if (!parents[i].isUseless) {
                main_stack[++top] = (StackItem){
                    .node_id = parents[i].node_id,
                    .node_id_popped = 1};
            }
        }
    }

    // Final output
    printf("\nShortest path cost: %d\nPath: ", path_state.best_path_cost);
    for (int i = path_state.best_path_size; i >= 0; i--) {
        printf("%u%s", path_state.best_path_stack[i],
               (i > 0 ? " -> " : "\n"));
    }
}
