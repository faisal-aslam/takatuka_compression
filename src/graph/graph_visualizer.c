#include "graph_visualizer.h"
#include "graph.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

static const char* LEVEL_COLORS[] = {
    "#CBA6F7", "#FFCDD2", "#F8BBD0", "#E1BEE7", "#D1C4E9",
    "#C5CAE9", "#BBDEFB", "#B2EBF2", "#B2DFDB", "#C8E6C9",
    "#DCEDC8", "#FFF9C4", "#FFE0B2", "#FFCCBC", "#D7CCC8"
};

static inline uint8_t useful_node(GraphNode* node, const uint8_t *block) {
    uint32_t freq = seq_freq_get(&block[node->offset], node->sequence_length); 
    if (node->sequence_length > 1 && freq == 1) return 0; //not useful.
    return 1;
}

static void print_node_content(FILE* output, const GraphNode* node, const uint8_t* block) {
    if (node->sequence_length == 0) {
        fprintf(output, "Root");
        return;
    }

    for (uint8_t i = 0; i < node->sequence_length; i++) {
       
        if (i > 0) fprintf(output, ",");
        fprintf(output, "%c", block[node->offset + i]);
    }
}

static const char* get_node_color(uint16_t level) {
    if (level == 0) return "#000000"; // Black for root
    size_t num_colors = sizeof(LEVEL_COLORS) / sizeof(LEVEL_COLORS[0]);
    return LEVEL_COLORS[level % num_colors];
}

static void print_node(FILE* output, const GraphNode* node, const uint8_t *block) {
    const char* fillcolor = get_node_color(node->node_level);
    const char* fontcolor = (node->node_id == 0) ? "white" : "black";

    fprintf(output, "    %d [label=\"%d\\n", node->node_id, node->node_id);
    print_node_content(output, node, block);
    if(!node->is_RLE) {
        fprintf(output, "\n l=%u", node->node_level);
    } else {
        fprintf(output, "\n l=%u, \nRLE", node->node_level);
    }
    fprintf(output, "\", shape=box, style=filled, fillcolor=\"%s\", fontcolor=\"%s\"];\n", 
            fillcolor, fontcolor);
}

static void print_links(FILE* output, const GraphNode* node, const uint8_t* block) {
    if (node->node_id == 0 ) return;

    uint16_t parent_count = get_parent_nodes_count((GraphNode*)node);
    GraphNode* parent_nodes = get_parent_nodes((GraphNode*)node);

    for (uint16_t i = 0; i < parent_count; i++) {
        GraphNode* parent = &parent_nodes[i];
        if (!useful_node(parent, block)) continue;    
        fprintf(output, "    %u -> %u;\n", node->node_id, parent->node_id);
        break; // Show only one parent per node
    }
}

void visualize_graph(const uint8_t* block) {
    FILE* output = fopen("./graph.dot", "w");
    if (!output) return;

    // Graphviz header
    fprintf(output, "digraph compression_graph {\n"
                    "  rankdir=BT;\n"
                    "  node [shape=box, style=filled];\n"
                    "  edge [fontsize=8];\n"
                    "  node [fontsize=10, width=0.5, height=0.3];\n"
                    "  nodesep=0.15; ranksep=0.25;\n\n");

    // First pass: nodes
    fprintf(output, "  // Nodes\n");
    for (uint32_t i = 0; i < get_graph_size(); i++) {
        GraphNode* node = get_graph_node(i);        
        if (node && useful_node(node, block)) {
            
            print_node(output, node, block);
        }
    }
    fprintf(output, "\n");

    // Second pass: rank by node_level
    fprintf(output, "  // Rank same by node_level\n");
    uint16_t max_level = get_last_level_index();
    for (uint16_t level = 0; level <= max_level; level++) {
        bool level_has_nodes = false;

        // Check if this level has any nodes
        for (uint32_t i = 0; i < get_graph_size(); i++) {
            GraphNode* node = get_graph_node(i);            
            if (node && node->node_level == level && useful_node(node, block)) {
                level_has_nodes = true;
                break;
            }
        }

        if (!level_has_nodes) {
            continue; // otherwise continue to the next level.
        }

        // Print rank group for this level
        fprintf(output, "  subgraph level_%u {\n    rank=same;\n", level);
        for (uint32_t i = 0; i < get_graph_size(); i++) {
            GraphNode *node = get_graph_node(i);
          
            if (node && node->node_level == level && useful_node(node, block)) {
                fprintf(output, "    %d;\n", node->node_id);
            }
        }
        fprintf(output, "  } // Level %u\n", level);
    }
    fprintf(output, "\n");

    // Third pass: edges
    fprintf(output, "  // Edges\n");
    for (uint32_t i = 0; i < get_graph_size(); i++) {
        GraphNode* node = get_graph_node(i);
        
        if (node && node->node_id != 0 && useful_node(node, block)) {
            print_links(output, node, block);
        }
    }

    fprintf(output, "}\n");
    fclose(output);
}
