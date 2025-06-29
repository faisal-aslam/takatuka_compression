// shortest_path.c
#include "shortest_path.h"
#include "graph.h"
#include <stdio.h>
#include <limits.h>

#define MAX_STACK_SIZE MAX_GRAPH_NODES

typedef struct {
    uint32_t node_id;
    uint32_t cost;
} StackItem;

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
void find_shortest_path_to_sink(void) {
    uint16_t last_level = get_last_level_index();
    uint32_t start = get_level_start_id(last_level);
    uint32_t end = get_level_end_id(last_level);

    // Stack for DFS
    StackItem stack[MAX_STACK_SIZE];
    int top = -1;

    // Push all leaf nodes from last level
    for (uint32_t i = start; i < end; i++) {        
        GraphNode *node = get_graph_node(i);
        if (node && node->isUseless) continue;
        stack[++top] = (StackItem){ .node_id = i, .cost = 0 };
    }

    while (top >= 0) {
        StackItem current = stack[top--];
        

        if (current.node_id == UINT32_MAX) {
            printf("pop data of %u\n", current.cost);
            continue;
        }
        GraphNode *node = get_graph_node(current.node_id);
        if (node->isUseless) continue;

        printf("visited = %u\n",node->node_id);
        
        
        
        if (node->node_id == 0) {
            printf(" saved the path with cost.\n");
            continue; // root has no parent
        }
        stack[++top] = (StackItem){ .node_id = UINT32_MAX, .cost = node->node_id };
        printf("push data of %u\n", current.node_id);

        uint8_t parent_count = get_parent_nodes_count(node);
        GraphNode* parents = get_parent_nodes(node);

        for (uint8_t i = 0; i < parent_count; i++) {
            GraphNode* parent_node = &parents[i];
            if (parent_node->isUseless) continue;
            stack[++top] = (StackItem){ .node_id = parent_node->node_id, .cost = 1 };        }
    }

}
