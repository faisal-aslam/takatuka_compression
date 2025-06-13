// graph_visualizer.c

#include "graph/graph_visualizer.h"
#include <stdlib.h>
#include <string.h>

void graphviz_init(GraphVisualizer *viz, const char *filename, bool overwrite) {
    if (!viz)
        return;

    if (overwrite) {
        viz->dot_file = fopen(filename, "w");
    } else {
        viz->dot_file = fopen(filename, "a");
    }

    if (!viz->dot_file) {
        perror("Failed to open Graphviz file");
        return;
    }

    // Don't write any graph declarations here
    viz->edge_count = 0;
    viz->edge_capacity = 0;
    viz->edges = NULL;
    viz->current_level = 0;
}

void graphviz_render_full_graph(GraphVisualizer *viz, const uint8_t *block) {
    if (!viz->dot_file)
        return;

    uint32_t max_level = get_max_level();

    // Write graph header (only once)
    fprintf(viz->dot_file, "digraph compression_graph {\n"
                           "  rankdir=TB;\n"
                           "  node [shape=record, style=filled];\n"
                           "  edge [fontsize=8];\n"
                           "  node [fontsize=10, width=0.5, height=0.3];\n"
                           "  nodesep=0.15; ranksep=0.25;\n");

    // First pass: Create all nodes
    for (uint32_t i = 0; i < graph.current_node_index; i++) {
        GraphNode *node = graph_get_node(i);
        if (!node)
            continue;

        char seq_label[512] = "";
        for (uint8_t j = 0; j < node->compress_sequence; j++) {
            char byte_str[10];
            snprintf(byte_str, sizeof(byte_str), "0x%02x",
                     block[node->compress_start_index + j]);
            strcat(seq_label, byte_str);
            if (j < node->compress_sequence - 1)
                strcat(seq_label, ", ");
        }

        const char *colors[] = {"#ffdddd", "#ddffdd", "#ddddff", "#ffffdd"};
        fprintf(viz->dot_file,
                "  node_%u [label=\"[%u]\\n%s\\nSavings: %d\", "
                "fillcolor=\"%s\"];\n",
                node->id, node->id, seq_label,
                node->saving_so_far, colors[node->level % 4]);
    }

    // Second pass: Create all edges
    for (uint32_t i = 0; i < graph.current_node_index; i++) {
        GraphNode *node = graph_get_node(i);
        if (!node)
            continue;

        for (uint8_t p = 0; p < node->parent_count; p++) {
            uint32_t parent_id = node->parents[p];
            GraphNode *parent = graph_get_node(parent_id);
            if (!parent)
                continue;

            fprintf(viz->dot_file,
                    "  node_%u -> node_%u [label=\"w:%u\", tailport=c, "
                    "headport=c];\n",
                    parent_id, node->id, node->incoming_weight);
        }
    }

    // Third pass: Create level groupings
    for (uint32_t level = 1; level <= max_level; level++) {
        fprintf(viz->dot_file, "  { rank=same; ");

        // Find all nodes at this level
        for (uint32_t i = 0; i < graph.current_node_index; i++) {
            GraphNode *node = graph_get_node(i);
            if (node && node->level == level) {
                fprintf(viz->dot_file, "node_%u; ", node->id);
            }
        }
        fprintf(viz->dot_file, "} /* level %u */\n", level);
    }

    // Close graph properly
    // fprintf(viz->dot_file, "}\n");
    fflush(viz->dot_file);
}

void graphviz_finalize(GraphVisualizer *viz) {
    if (viz->dot_file) {
        fprintf(viz->dot_file, "}\n");
        fclose(viz->dot_file);
    }
    free(viz->edges);
}

/*
#include "graph_visualizer.h"

int main() {
GraphVisualizer viz;
graphviz_init(&viz, "output.dot", true);

// For each level in your graph building:
GraphNode* current_level_nodes =  //your nodes ;
uint32_t count =  //node count ;
graphviz_add_level(&viz, current_level_nodes, count, block);

graphviz_finalize(&viz);
    return 0;
}

*/