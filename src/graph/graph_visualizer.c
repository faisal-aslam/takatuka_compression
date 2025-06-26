#include "graph_visualizer.h"
#include "graph.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// Color palette for levels (15 distinct colors)
static const char* LEVEL_COLORS[] = {
    "black",        // Level 0 (root)
    "lightblue",    // Level 1
    "lightgreen",   // Level 2
    "lightyellow",  // Level 3
    "lightpink",    // Level 4
    "lavender",     // Level 5
    "tan",          // Level 6
    "thistle",      // Level 7
    "wheat",        // Level 8
    "palegoldenrod",// Level 9
    "mintcream",    // Level 10
    "aliceblue",    // Level 11
    "honeydew",     // Level 12
    "azure",        // Level 13
    "seashell"      // Level 14
};

static void print_node_content(FILE* output, const GraphNode* node, const uint8_t* block) {
    if (node->sequence_length == 0) {
        fprintf(output, "Root");
        return;
    }

    for (uint8_t i = 0; i < node->sequence_length; i++) {
        if (i > 0) fprintf(output, ",");
        fprintf(output, "%d", block[node->start_of_sequence + i]);
    }
}

static const char* get_node_color(uint16_t level) {
    if (level >= sizeof(LEVEL_COLORS)/sizeof(LEVEL_COLORS[0])) {
        return "white"; // Default if we exceed palette
    }
    return LEVEL_COLORS[level];
}

static void print_node(FILE* output, const GraphNode* node, const uint8_t *block) {
    uint16_t level = node->node_id / MAX_WEIGHTS;
    const char* color = get_node_color(level);
    const char* fontcolor = (level == 0) ? "white" : "black";

    fprintf(output, "    %d [label=\"%d\\n", node->node_id, node->node_id);
    print_node_content(output, node, block);
    fprintf(output, "\", shape=box, style=filled, fillcolor=\"%s\", fontcolor=\"%s\"];\n", 
            color, fontcolor);
}

static void print_links(FILE* output, const GraphNode* node) {
    for (uint8_t i = 0; i < node->parent_count; i++) {
        ParentLink link = node->parent_link[i];
        fprintf(output, "    %d -> %d [label=\"%d\"];\n", 
                node->node_id, link.parent_id, link.cost);
    }
}

void visualize_graph(const uint8_t* block) {
    const Graph* graph = get_graph();
     FILE* output = fopen("./graph.dot","w"); 
    if (!graph || !output) return;
    
    fprintf(output, "digraph G {\n");
    fprintf(output, "    rankdir=TB;\n");
    fprintf(output, "    node [fontname=\"Arial\"];\n\n");
    
    // Print only active nodes
    for (uint16_t level = 0; level < graph->current_level; level++) {
        bool level_has_nodes = false;
        
        // Check if level has any nodes first
        for (uint16_t weight = 0; weight < MAX_WEIGHTS; weight++) {
            uint32_t node_id = level * MAX_WEIGHTS + weight;
            GraphNode* node = get_graph_node(node_id);
            if (node && node->sequence_length > 0) {
                level_has_nodes = true;
                break;
            }
        }
        
        if (!level_has_nodes) continue;
        
        // Print nodes for this level
        fprintf(output, "    subgraph cluster_%d {\n", level);
        fprintf(output, "        style=invis;\n");
        
        for (uint16_t weight = 0; weight < MAX_WEIGHTS; weight++) {
            uint32_t node_id = level * MAX_WEIGHTS + weight;
            GraphNode* node = get_graph_node(node_id);
            if (node && node->sequence_length > 0) {
                print_node(output, node, block);
            }
        }
        
        fprintf(output, "    }\n\n");
        
        // Create rank for this level
        fprintf(output, "    { rank=same; ");
        for (uint16_t weight = 0; weight < MAX_WEIGHTS; weight++) {
            uint32_t node_id = level * MAX_WEIGHTS + weight;
            GraphNode* node = get_graph_node(node_id);
            if (node && node->sequence_length > 0) {
                fprintf(output, "%d; ", node_id);
            }
        }
        fprintf(output, "}\n");
    }
    
    // Print links for active nodes
    for (uint32_t i = 0; i < TOTAL_NODES; i++) {
        GraphNode* node = get_graph_node(i);
        if (node && node->sequence_length > 0 && node->parent_count > 0) {
            print_links(output, node);
        }
    }
    
    fprintf(output, "}\n");
}