// tree_visualizer.h
#ifndef TREE_VISUALIZER_H
#define TREE_VISUALIZER_H

#include "tree_node.h"
#include <stdio.h>
#include <stdbool.h>

typedef struct {
    FILE* dot_file;
    int current_level;
    bool show_pruned;
} TreeVisualizer;

void init_visualizer(TreeVisualizer* viz, const char* filename, bool show_pruned);
void visualize_add_level(TreeVisualizer* viz, TreeNode* nodes, int node_count,
                       TreeNode* old_node, const uint8_t* block, uint32_t block_index);
void visualize_mark_pruned(TreeVisualizer* viz, TreeNode* level_nodes, int node_count);
void finalize_visualizer(TreeVisualizer* viz);

#endif