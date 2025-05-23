#include "graph/graph_visualizer.h"
#include <stdlib.h>
#include <string.h>

// Helper to escape special characters in DOT labels
static void escape_label(char* dest, const char* src, size_t max_len) {
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j < max_len - 1; i++) {
        switch(src[i]) {
            case '\"': if (j+2 < max_len) { dest[j++] = '\\'; dest[j++] = '\"'; } break;
            case '\\': if (j+2 < max_len) { dest[j++] = '\\'; dest[j++] = '\\'; } break;
            default: dest[j++] = src[i];
        }
    }
    dest[j] = '\0';
}

void graphviz_init(GraphVisualizer* viz, const char* filename, bool show_all) {
    viz->dot_file = fopen(filename, "w");
    viz->current_level = 0;
    viz->show_all = show_all;
    viz->edges = NULL;
    viz->edge_count = 0;
    viz->edge_capacity = 0;

    if (viz->dot_file) {
        fprintf(viz->dot_file, "digraph compression_graph {\n");
        fprintf(viz->dot_file, "  rankdir=TB;\n");
        fprintf(viz->dot_file, "  node [shape=record, style=filled];\n");
        fprintf(viz->dot_file, "  edge [fontsize=8];\n");
        fprintf(viz->dot_file, "  node [fontsize=10, width=0.5, height=0.3];\n");
        fprintf(viz->dot_file, "  nodesep=0.15; ranksep=0.25;\n");
    }
}

void graphviz_add_level(GraphVisualizer* viz, GraphNode* nodes, uint32_t node_count, const uint8_t* block) {
    if (!viz->dot_file) return;

    // Create nodes
    for (uint32_t i = 0; i < node_count; i++) {
        GraphNode* node = &nodes[i];
        
        char seq_label[512] = "";
        for (uint8_t j = 0; j < node->compress_sequence; j++) {
            char byte_str[10];
            snprintf(byte_str, sizeof(byte_str), "0x%02x", 
                    block[node->compress_start_index + j]);
            strcat(seq_label, byte_str);
            if (j < node->compress_sequence - 1) strcat(seq_label, ", ");
        }

        char escaped_label[1024];
        escape_label(escaped_label, seq_label, sizeof(escaped_label));

        const char* colors[] = {"#ffdddd", "#ddffdd", "#ddddff", "#ffffdd"};
        fprintf(viz->dot_file,
            "  node_%u [label=\"[%u]\\n%s\\nSavings: %d\", "
            "fillcolor=\"%s\"];\n",
            node->id, node->compress_sequence, escaped_label, 
            node->saving_so_far, colors[viz->current_level % 4]);
    }

    // Create edges
    for (uint32_t i = 0; i < node_count; i++) {
        GraphNode* node = &nodes[i];
        
        for (uint8_t p = 0; p < node->parent_count; p++) {
            uint32_t parent_id = node->parents[p];
            
            bool exists = false;
            for (size_t j = 0; j < viz->edge_count; j++) {
                if (viz->edges[j].parent_id == parent_id && 
                    viz->edges[j].child_id == node->id) {
                    exists = true;
                    break;
                }
            }
            
            if (!exists) {
                if (viz->edge_count >= viz->edge_capacity) {
                    viz->edge_capacity = viz->edge_capacity ? viz->edge_capacity * 2 : 64;
                    viz->edges = realloc(viz->edges, viz->edge_capacity * sizeof(GraphEdge));
                }
                
                viz->edges[viz->edge_count++] = (GraphEdge){parent_id, node->id};
                fprintf(viz->dot_file, "  node_%u -> node_%u [label=\"w:%u\"];\n",
                    parent_id, node->id, node->incoming_weight);
            }
        }
    }

    // Level alignment
    fprintf(viz->dot_file, "  { rank=same; ");
    for (uint32_t i = 0; i < node_count; i++) {
        fprintf(viz->dot_file, "node_%u; ", nodes[i].id);
    }
    fprintf(viz->dot_file, "}\n");
    
    viz->current_level++;
}

void graphviz_finalize(GraphVisualizer* viz) {
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