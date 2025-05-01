// tree_visualizer.h
#ifndef TREE_VISUALIZER_H
#define TREE_VISUALIZER_H

#include "tree_node.h"
#include <stdio.h>
#include <stdbool.h>

typedef struct {
    uint32_t parent_id;
    uint32_t child_id;
} Edge;

typedef struct {
    FILE* dot_file;
    int current_level;
    bool show_pruned;
    uint32_t next_node_id;
    Edge* edges;          // Track edges to prevent duplicates
    size_t edge_count;    // Current number of edges
    size_t edge_capacity; // Allocated capacity
} TreeVisualizer;


void init_visualizer(TreeVisualizer* viz, const char* filename, bool show_pruned);
void visualize_add_level(TreeVisualizer* viz, TreeNode* nodes, int node_count, const uint8_t* block, uint32_t block_index);
void finalize_visualizer(TreeVisualizer* viz);

#endif