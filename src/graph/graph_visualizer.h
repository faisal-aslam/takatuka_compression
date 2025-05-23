#ifndef GRAPH_VISUALIZER_H
#define GRAPH_VISUALIZER_H

#include "graph.h"
#include <stdio.h>
#include <stdbool.h>

typedef struct {
    uint32_t parent_id;
    uint32_t child_id;
} GraphEdge;

typedef struct {
    FILE* dot_file;
    uint32_t current_level;
    bool show_all;
    GraphEdge* edges;
    size_t edge_count;
    size_t edge_capacity;
} GraphVisualizer;

void graphviz_init(GraphVisualizer* viz, const char* filename, bool show_all);
void graphviz_add_level(GraphVisualizer* viz, uint32_t start_index, uint32_t end_index, const uint8_t* block);
void graphviz_finalize(GraphVisualizer* viz);


#endif