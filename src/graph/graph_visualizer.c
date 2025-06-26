#include "graph_visualizer.h"
#include "graph.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

static const char* LEVEL_COLORS[] = {
    "#000000",  // Level 0 - Root (black)

    "#FFCDD2",  // Level 1 - Light Red / Rose
    "#F8BBD0",  // Level 2 - Light Pink
    "#E1BEE7",  // Level 3 - Light Purple
    "#D1C4E9",  // Level 4 - Soft Lavender
    "#C5CAE9",  // Level 5 - Light Indigo
    "#BBDEFB",  // Level 6 - Light Sky Blue
    "#B2EBF2",  // Level 7 - Aqua / Cyan
    "#B2DFDB",  // Level 8 - Light Teal
    "#C8E6C9",  // Level 9 - Light Green
    "#DCEDC8",  // Level 10 - Pale Lime
    "#FFF9C4",  // Level 11 - Light Yellow
    "#FFE0B2",  // Level 12 - Light Orange
    "#FFCCBC",  // Level 13 - Soft Salmon
    "#D7CCC8"   // Level 14 - Soft Brown
};

static void print_node_content(FILE* output, const GraphNode* node, const uint8_t* block) {
    if (node->sequence_length == 0) {
        fprintf(output, "Root");
        return;
    }

    for (uint8_t i = 0; i < node->sequence_length; i++) {
        if (i > 0) fprintf(output, ",");
        fprintf(output, "%0x", block[node->start_of_sequence + i]);
    }
}

static const char* get_node_color(uint16_t level) {
    if (level >= sizeof(LEVEL_COLORS)/sizeof(LEVEL_COLORS[0])) {
        return "white"; // Default if we exceed palette
    }
    return LEVEL_COLORS[level];
}

static void print_node(FILE* output, const GraphNode* node, const uint8_t *block) {
    // Determine level of node by scanning level starts
    uint16_t level = 0;
    for (uint16_t i = 0; i <= get_total_levels(); i++) {
        if (node->node_id >= get_level_start_id(i) && node->node_id < get_level_end_id(i)) {
            level = i;
            break;
        }
    }

    const char* fillcolor;
    const char* fontcolor;

   
    fillcolor = get_node_color(level);
    fontcolor = "black";
    

    fprintf(output, "    %d [label=\"%d\\n", node->node_id, node->node_id);
    print_node_content(output, node, block);
    fprintf(output, "\", shape=box, style=filled, fillcolor=\"%s\", fontcolor=\"%s\"];\n", 
            fillcolor, fontcolor);
}


static void print_links(FILE* output, const GraphNode* node) {
    for (uint8_t i = 0; i < node->parent_count; i++) {
        ParentLink link = node->parent_link[i];
        fprintf(output, "    %d -> %d [label=\"%d\"];\n", 
                node->node_id, link.parent_id, link.cost);
    }
}

void visualize_graph(const uint8_t* block) {    
     FILE* output = fopen("./graph.dot","w"); 
    if (!output) return;
    
    fprintf(output, "digraph compression_graph {\n"
                           "  rankdir=BT;\n"
                           "  node [shape=record, style=filled];\n"
                           "  edge [fontsize=8];\n"
                           "  node [fontsize=10, width=0.5, height=0.3];\n"
                           "  nodesep=0.15; ranksep=0.25;\n");
    
    // Print only active nodes
    for (uint16_t level = 0; level <= get_total_levels(); level++) {
        bool level_has_nodes = false;
        
        // Print nodes for this level
        fprintf(output, "    subgraph cluster_%d {\n", level);
        fprintf(output, "        style=invis;\n");
        
        for (uint8_t i = get_level_start_id(level); i < get_level_end_id(level); i++) {        
            GraphNode* node = get_graph_node(i);
            if (node && node->sequence_length > 0) {
                print_node(output, node, block);
            }
        }
        
        fprintf(output, "    }\n\n");
        
        // Create rank for this level
        fprintf(output, "    { rank=same; ");
        for (uint8_t i = get_level_start_id(level); i < get_level_end_id(level); i++) {        
            GraphNode* node = get_graph_node(i);
            if (node && node->sequence_length > 0) {
                fprintf(output, "%d; ", node->node_id);
            }
        }
        fprintf(output, "}\n");
    }
    
    // Print links for active nodes
    for (uint32_t i = 0; i < get_graph_size(); i++) {
        GraphNode* node = get_graph_node(i);
        if (node && node->sequence_length > 0 && node->parent_count > 0) {
            print_links(output, node);
        }
    }
    
    fprintf(output, "}\n");
}