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

void find_shortest_path_to_sink(void) {
    uint16_t last_level = get_last_level_index();
    uint32_t start = get_level_start_id(last_level);
    uint32_t end = get_level_end_id(last_level);

    // Distance and parent tracking
    uint32_t distance[MAX_GRAPH_NODES];
    uint32_t parent[MAX_GRAPH_NODES];

    for (uint32_t i = 0; i < get_graph_node_count(); i++) {
        GraphNode* node = get_graph_node(i);
        if (node && node->isUseless) continue;
        distance[i] = UINT32_MAX;
        parent[i] = UINT32_MAX;
    }

    // Stack for DFS
    StackItem stack[MAX_STACK_SIZE];
    int top = -1;

    // Push all leaf nodes from last level
    for (uint32_t i = start; i < end; i++) {        
        stack[++top] = (StackItem){ .node_id = i, .cost = 0 };
        distance[i] = 0;
    }

    while (top >= 0) {
        StackItem current = stack[top--];
        GraphNode *node = get_graph_node(current.node_id);

        if (node->isUseless) continue;

        if (node->node_id == 0) continue; // root has no parent

        uint8_t parent_count = get_parent_nodes_count(node);
        GraphNode* parents = get_parent_nodes(node);

        for (uint8_t i = 0; i < parent_count; i++) {
            GraphNode* parent_node = &parents[i];
            if (parent_node->isUseless) continue;

            uint32_t new_cost = current.cost + 1;
            if (new_cost < distance[parent_node->node_id]) {
                distance[parent_node->node_id] = new_cost;
                parent[parent_node->node_id] = node->node_id;
                stack[++top] = (StackItem){ .node_id = parent_node->node_id, .cost = new_cost };
            }
        }
    }

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
