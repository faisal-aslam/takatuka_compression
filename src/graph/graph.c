#include "graph.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "graph_visualizer.h"

static Graph graph;

static inline uint16_t get_parent_level(GraphNode* node);


void init_graph(void) {
    graph.nodes = malloc(INITIAL_GRAPH_NODES * sizeof(GraphNode));
    assert(graph.nodes && "Failed to allocate memory for graph nodes.");

    graph.capacity = INITIAL_GRAPH_NODES;
    graph.size = 0;
    graph.total_levels = 0;    
    memset(graph.first_node_of_level, 0, sizeof(graph.first_node_of_level));
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
    g_node->node_id = graph.size-1; //please never change node's id ever.
    g_node->node_level = graph.total_levels-1; //please do not change this ever too.
    g_node->sequence_length = 0;
    g_node->offset = 0;
    
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

uint16_t get_last_level_index(void)  {
    return (graph.total_levels > 0) ? graph.total_levels - 1 : 0;
}

static inline uint16_t get_parent_level(GraphNode* node) {
    uint16_t parent_level =  node->node_level-node->sequence_length; 
    if (parent_level == UINT16_MAX ||  parent_level > MAX_LEVELS) {
        fprintf(stderr, "illegal parent level");
        exit(1);
    }
    return parent_level;
}
/*void add_link_to_parent(GraphNode* child_node, GraphNode* parent_node, uint8_t cost) {
    if (!child_node || !parent_node || child_node->parent_count >= MAX_WEIGHTS) 
        return;

    child_node->parent_link[child_node->parent_count++] = (ParentLink){
        .parent_id = parent_node->node_id,
        .cost = cost
    };
}*/

uint32_t get_graph_size(void) {
    return graph.size;
}

GraphNode* get_graph_node(uint32_t node_id) {
    assert(node_id < graph.size);
    return &graph.nodes[node_id];
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

static inline void print_node_link(GraphNode* node, GraphNode* parent) {
    printf("\t %u --> %u\n", node->node_id, parent->node_id);
}

uint32_t total_nodes_at_level(uint16_t level) {
    return (get_level_end_id(level) - get_level_start_id(level));
}

uint8_t get_parent_nodes_count(GraphNode* node) {
    if (!node || node->node_id == 0) {
        return 0;
    }
    uint8_t parents_count = total_nodes_at_level(get_parent_level(node));
    if (parents_count > SEQ_LENGTH_LIMIT) {
        fprintf(stderr, "Illegal number of parent nodes");
        exit(1);
    }
    return parents_count;
}

GraphNode* get_parent_nodes(GraphNode* node) {
    //Step 1: Get parent level.
    uint16_t parent_level = get_parent_level(node);
    // Step 2: Get the index of the first node of the parent level
    uint32_t start_index =get_level_start_id (parent_level);
    return &graph.nodes[start_index];
}

void print_graph_node(GraphNode *node) {
    if (!node) return;

    if (node->node_id == 0) {
        printf("\nROOT NODE ");
    } else {
        printf("\n");
    }
    uint16_t parent_nodes_count = get_parent_nodes_count(node);
    printf("node_id = %u, start_of_sequence = %u, sequence_length = %u",
           node->node_id, node->offset, node->sequence_length);
    printf(", parent_count = %u\n", parent_nodes_count);
    GraphNode* parent_nodes = get_parent_nodes(node);
    if (!parent_nodes) return;
    for (int i = 0; i < parent_nodes_count; i++) {
        print_node_link(node, &parent_nodes[i]);
    }

}
