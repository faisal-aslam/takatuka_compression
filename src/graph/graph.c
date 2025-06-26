#include "graph.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "graph_visualizer.h"

static Graph graph;

void init_graph(void) {
    graph.nodes = malloc(INITIAL_GRAPH_NODES * sizeof(GraphNode));
    assert(graph.nodes && "Failed to allocate memory for graph nodes.");

    graph.capacity = INITIAL_GRAPH_NODES;
    graph.size = 0;
    graph.total_levels = 0;    
    memset(graph.first_node_of_level, 0, sizeof(graph.first_node_of_level));
}

static inline void init_graph_node(GraphNode* g_node) {
    g_node->node_id = graph.size-1;
    g_node->parent_count = 0;
    g_node->sequence_length = 0;
    g_node->start_of_sequence = 0;
}

GraphNode* get_next_node(void) {
    if (graph.size == graph.capacity) {
        // Reallocate memory while preserving existing nodes
        uint32_t new_capacity = graph.capacity * GROWTH_FACTOR;
        GraphNode* new_nodes = realloc(graph.nodes, new_capacity * sizeof(GraphNode));
        assert(new_nodes && "Failed to grow graph memory.");
        graph.nodes = new_nodes;
        graph.capacity = new_capacity;
    }

    GraphNode* g_node = &graph.nodes[graph.size++];
    init_graph_node(g_node);
    return g_node;
}

uint32_t get_level_start_id(uint16_t level) {
    if (level >= graph.total_levels) {
        fprintf(stderr, "Illegal level: %u (total_levels=%u)\n", level, graph.total_levels);
        exit(1);
    }
    return graph.first_node_of_level[level];
}

uint32_t get_level_end_id(uint16_t level) {
    if (level >= graph.total_levels) {
        fprintf(stderr, "Illegal level: %u (total_levels=%u)\n", level, graph.total_levels);
        exit(1);
    }
    if (level == graph.total_levels - 1) {
        return graph.size;
    }
    return graph.first_node_of_level[level + 1];
}

uint16_t get_total_levels(void) {
    return graph.total_levels-1;
}

void add_link_to_parent(GraphNode* child_node, GraphNode* parent_node, uint8_t cost) {
    if (!child_node || !parent_node || child_node->parent_count >= MAX_WEIGHTS) 
        return;

    child_node->parent_link[child_node->parent_count++] = (ParentLink){
        .parent_id = parent_node->node_id,
        .cost = cost
    };
}

uint32_t get_graph_size(void) {
    return graph.size;
}

GraphNode* get_graph_node(uint32_t node_id) {
    return (node_id < graph.size) ? &graph.nodes[node_id] : NULL;
}

void increment_graph_level(void) {
    if (graph.total_levels < MAX_LEVELS) {
        graph.first_node_of_level[graph.total_levels] = graph.size;
        graph.total_levels++;
    }
}

uint32_t get_graph_node_count(void) {
    return graph.size;
}

static inline void print_node_link(GraphNode* node, ParentLink* link) {
    printf("\t %u --> %u\n", node->node_id, link->parent_id);
}

void print_graph_node(GraphNode *node) {
    if (!node) return;

    if (node->node_id == 0) {
        printf("\nROOT NODE ");
    } else {
        printf("\n");
    }

    printf("node_id = %u, start_of_sequence = %u, sequence_length = %u",
           node->node_id, node->start_of_sequence, node->sequence_length);
    printf(", parent_count = %u\n", node->parent_count);

    for (int i = 0; i < node->parent_count; i++) {
        print_node_link(node, &node->parent_link[i]);
    }
}
