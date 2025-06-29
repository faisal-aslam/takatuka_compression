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
    uint32_t current_path_size;
    int32_t current_path_cost;
    uint32_t best_path_stack[MAX_LEVELS];
    int32_t best_path_cost;
    uint32_t best_path_size;
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
static inline void add_in_data_of_path(GraphNode* node, const uint8_t* block) {
    printf("push data of %u\n", node->node_id);
    uint32_t freq_after_inc = seq_repo_increase_frequency(&block[node->offset], node->sequence_length);
}

static inline void remove_data_of_path(StackItem current,
                                       const uint8_t *block) {
    printf("pop data of %u\n", current.node_id_popped);
    GraphNode *node = current.node_id_popped;
    seq_repo_decrease_frequency(&block[node->offset], node->sequence_length);
    path_state.current_path_size--;
}


static inline void update_best_path() {
    if (path_state.current_path_cost < path_state.best_path_cost) {
        path_state.best_path_cost = path_state.current_path_cost;
        memcpy(path_state.best_path_stack, path_state.current_path_cost,
               path_state.current_path_size * sizeof(uint32_t));
        path_state.best_path_size = path_state.current_path_size;
    }
}

static inline void print_current_path() {
    for (int stack_index = 0; stack_index <= path_state.current_path_size;
         stack_index++) {
        printf("%u", path_state.current_path_stack[stack_index]);
        if (stack_index + 1 <= path_state.current_path_size) {
            printf(" -> ");
        }
        fflush(stdout);
    }
    printf("\n");
}

void find_shortest_path_to_sink(const uint8_t* block) {
    uint16_t last_level = get_last_level_index();
    uint32_t start = get_level_start_id(last_level);
    uint32_t end = get_level_end_id(last_level);

    // Stack for DFS
    StackItem stack[MAX_STACK_SIZE];
    int top = -1;

    // Push all (useful) leaf nodes from last level
    // We never push useless nodes in the stack and never check them while poping.
    for (uint32_t i = start; i < end; i++) {        
        GraphNode *node = get_graph_node(i);
        if (node && node->isUseless) continue;
        stack[++top] = (StackItem){ .node_id = i, .node_id_popped = 0 };
    }

    while (top >= 0) {
        StackItem current = stack[top--];        

        //This is the point where all the links of node_id_popped has been explored.
        //Thus, here we pop its data.
        if (current.node_id == UINT32_MAX) { //a special id to check node_id_popped.
            remove_data_of_path(current, block);
            continue;
        }
        GraphNode *node = get_graph_node(current.node_id);
        
        path_state.current_path_stack[++path_state.current_path_size] = node->node_id;

               
        if (node->node_id == 0) {
            printf(" saved the path with cost.\n");
            print_current_path();
            update_best_path();
        }
        stack[++top] = (StackItem){ .node_id = UINT32_MAX, .node_id_popped = node->node_id };
        //recording data of a node.
        add_in_data_of_path(node, block);
        

        if (node->node_id == 0) continue; //root has no parents continue;
        uint8_t parent_count = get_parent_nodes_count(node);
        GraphNode* parents = get_parent_nodes(node);

        for (uint8_t i = 0; i < parent_count; i++) {
            GraphNode* parent_node = &parents[i];
            if (parent_node->isUseless) continue;
            stack[++top] = (StackItem){ .node_id = parent_node->node_id, .node_id_popped = 1 };        }
    }

}
