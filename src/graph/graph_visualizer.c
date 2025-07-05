#include "graph_visualizer.h"
#include "graph.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

static const char* LEVEL_COLORS[] = {
    "#CBA6F7",  // 
    "#FFCDD2",  // Light Red
    "#F8BBD0",  // Pink
    "#E1BEE7",  // Light Purple
    "#D1C4E9",  // Lavender
    "#C5CAE9",  // Light Indigo
    "#BBDEFB",  // Sky Blue
    "#B2EBF2",  // Aqua / Cyan
    "#B2DFDB",  // Teal
    "#C8E6C9",  // Light Green
    "#DCEDC8",  // Lime
    "#FFF9C4",  // Light Yellow
    "#FFE0B2",  // Light Orange
    "#FFCCBC",  // Light Salmon
    "#D7CCC8"   // Soft Brown
};

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
    if (level == 0) {
        return "#000000";
    }
    size_t num_colors = sizeof(LEVEL_COLORS) / sizeof(LEVEL_COLORS[0]);
    return LEVEL_COLORS[level % num_colors];
}



static void print_node(FILE* output, const GraphNode* node, const uint8_t *block) {
    // Determine level of node by scanning level starts
    uint16_t level = 0;
    for (uint16_t i = 0; i <= get_last_level_index(); i++) {
        if (node->node_id >= get_level_start_id(i) && node->node_id < get_level_end_id(i)) {
            level = i;
            break;
        }
    }

    const char* fillcolor;
    const char* fontcolor;

   
    fillcolor = get_node_color(level);
    fontcolor = "black";
    if (node->node_id == 0) fontcolor ="white"; //its root.

    fprintf(output, "    %d [label=\"%d\\n", node->node_id, node->node_id);
    print_node_content(output, node, block);
    fprintf(output, "\n d=%u,l=%u", node->min_depth, node->node_level);
    fprintf(output, "\", shape=box, style=filled, fillcolor=\"%s\", fontcolor=\"%s\"];\n", 
            fillcolor, fontcolor);
}


static void print_links(FILE* output, const GraphNode* node) {
    //output = stdout;
    if (node->node_id == 0) return;  // Root node has no parents

    uint16_t parent_count = get_parent_nodes_count((GraphNode*)node); // safe cast
    GraphNode* parent_nodes = get_parent_nodes((GraphNode*)node);

    for (uint16_t i = 0; i < parent_count; i++) {
        GraphNode* parent = &parent_nodes[i];    
        fprintf(output, " %u -> %u ;\n", 
                node->node_id, parent->node_id);
        
        break;// only create one link as all links points to the node of this level.
    }
}

void visualize_graph(const uint8_t* block) {    
     FILE* output = fopen("./graph.dot","w");  //or stdout
    if (!output) return;
    
    fprintf(output, "digraph compression_graph {\n"
                           "  rankdir=BT;\n"
                           "  node [shape=record, style=filled];\n"
                           "  edge [fontsize=8];\n"
                           "  node [fontsize=10, width=0.5, height=0.3];\n"
                           "  nodesep=0.15; ranksep=0.25;\n");
    
    // Print all nodes 
    for (uint16_t level = 0; level <= get_last_level_index(); level++) {
        
        // Print nodes for this level
        fprintf(output, "    subgraph cluster_%d {\n", level);
        fprintf(output, "        style=invis;\n");
        
        for (uint32_t i = get_level_start_id(level); i < get_level_end_id(level); i++) {        
            GraphNode* node = get_graph_node(i);              
            print_node(output, node, block);            
        }
        
        if (level % 20 == 0) fflush(output);
        fprintf(output, "    }\n\n");
        
        // Create rank for this level
        fprintf(output, "    { rank=same; ");
        for (uint32_t i = get_level_start_id(level); i < get_level_end_id(level); i++) {        
            GraphNode* node = get_graph_node(i);
            //if (node->isUseless) continue;
            if (node && node->sequence_length > 0) {
                fprintf(output, "%d; ", node->node_id);
            }
        }
        fprintf(output, "}\n");
    }

    // Print virtual links of nodes
    for (uint32_t i = 0; i < get_graph_size(); i++) {
        GraphNode *node = get_graph_node(i);
        if (node && node->node_id != 0) { // Skip root node (no parents)
            if (get_parent_nodes_count(node) > 0) {
                print_links(output, node);
            }
        }
        if (i%1000 == 0) fflush(output);
    }

    fprintf(output, "}\n");
    fflush(output);
    
}