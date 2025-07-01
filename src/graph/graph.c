#include "graph.h"

Graph graph; // Actual single definition

void init_graph(void) {
    graph.size = 0;
    graph.total_levels = 0;    
    memset(graph.first_node_of_level, 0, sizeof(graph.first_node_of_level));
}

// Verification function of the graph.
void verify_graph_integrity() {
    for (uint32_t i = 1; i < graph.size; i++) {
        GraphNode* node = &graph.nodes[i];
        uint16_t parent_level = node->node_level - node->sequence_length;
        if (parent_level >= graph.total_levels) {
            fprintf(stderr, "Node %u has invalid parent level %u\n",
                   node->node_id, parent_level);
            abort();
        }
    }
}

/**
 * Compacts the graph by removing useless nodes and updating level indices.
 * Operates in O(n) time with single pass through nodes.
 * Maintains correct node ordering and updates first_node_of_level array.
 */
void compact_graph(void) {
    // Validate root node exists and isn't useless
    assert(graph.size == 0 || (graph.nodes[0].node_id == 0 && !graph.nodes[0].isUseless));
    printf("\n\n graph size = %u", graph.size);
    uint32_t write_idx = 0;
    uint32_t current_level = 0;
    uint32_t level_start = 0;

    for (uint32_t read_idx = 0; read_idx < graph.size; read_idx++) {
        // Check for level transition
        if (current_level + 1 < graph.total_levels) {
            uint32_t next_level_start = graph.first_node_of_level[current_level + 1];
            if (read_idx >= next_level_start) {
                graph.first_node_of_level[current_level] = level_start;
                current_level++;
                level_start = write_idx;
            }
        }

        GraphNode* node = &graph.nodes[read_idx];
        if (node->isUseless) {
            continue;
        }

        if (write_idx != read_idx) {
            graph.nodes[write_idx] = *node;
            graph.nodes[write_idx].node_id = write_idx;
        }

        write_idx++;
    }

    // Update final level boundary
    graph.first_node_of_level[current_level] = level_start;

    // Handle any remaining empty levels
    while (++current_level < graph.total_levels) {
        graph.first_node_of_level[current_level] = write_idx;
    }

    graph.size = write_idx;
    printf("\n\n graph size = %u\n", graph.size);
    // Optional verification
    #ifdef DEBUG
    verify_graph_integrity();
    #endif
}

static inline void print_node_link(GraphNode* node, GraphNode* parent) {
    printf("\t %u --> %u\n", node->node_id, parent->node_id);
}

void print_node_sequence(GraphNode *node, const uint8_t* block) {
    for (int i=0; i<node->sequence_length; i++) {
        printf("%0x", block[node->offset+i]);
    }
    //printf("\n");
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