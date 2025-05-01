// tree_visualizer.c
#include "tree_visualizer.h"
#include "tree_node.h"
#include <string.h>
#include <stdlib.h>

// Helper function to escape special DOT characters
static void escape_dot_chars(char* dest, const char* src, size_t max_len) {
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j < max_len - 1; i++) {
        switch(src[i]) {
            case '\"': 
                if (j + 2 < max_len) { dest[j++] = '\\'; dest[j++] = '\"'; }
                break;
            case '\\':
                if (j + 2 < max_len) { dest[j++] = '\\'; dest[j++] = '\\'; }
                break;
            case '\n':
                if (j + 2 < max_len) { dest[j++] = '\\'; dest[j++] = 'n'; }
                break;
            default:
                dest[j++] = src[i];
        }
    }
    dest[j] = '\0';
}

void init_visualizer(TreeVisualizer *viz, const char *filename, bool show_pruned) {
    viz->dot_file = fopen(filename, "w");
    viz->current_level = 0;
    viz->show_pruned = show_pruned;
    viz->next_node_id = 0;
    viz->edges = NULL;
    viz->edge_count = 0;
    viz->edge_capacity = 0;
    
    if (viz->dot_file) {
        fprintf(viz->dot_file, "digraph compression_tree {\n");
        fprintf(viz->dot_file, "  rankdir=TB;\n");
        fprintf(viz->dot_file, "  node [shape=record, style=filled];\n");
    }
}

void visualize_add_level(TreeVisualizer *viz, TreeNode *nodes, int node_count,
                         TreeNode *old_node, const uint8_t *block, uint32_t block_index) {
    if (!viz->dot_file)
        return;

    // First pass: Create all nodes
    for (int i = 0; i < node_count; i++) {
        TreeNode *node = &nodes[i];
        if (node->isPruned && !viz->show_pruned)
            continue;

        struct SequenceRange range = get_sequence_range(node, block, block_index);
        char seq_label[256] = "[INVALID]";
        char escaped_label[512] = "";

        if (range.valid) {
            char *ptr = seq_label;
            int remaining = sizeof(seq_label);
            int written = 0;

            for (uint32_t j = range.start; j < range.end && remaining > 0; j++) {
                written = snprintf(ptr, remaining, "0x%02x", block[j]);
                if (written < 0 || written >= remaining)
                    break;
                ptr += written;
                remaining -= written;

                if (j < range.end && remaining > 0) {
                    *ptr++ = ',';
                    *ptr++ = ' ';
                    remaining -= 2;
                }
            }
            *ptr = '\0';
            escape_dot_chars(escaped_label, seq_label, sizeof(escaped_label));
        }

        const char *fillcolor = node->isPruned ? "fillcolor=\"#ffdddd\"" : "fillcolor=\"#ddffdd\"";
        const char *penwidth = node->isPruned ? "penwidth=1" : "penwidth=2";

        fprintf(viz->dot_file, "  node_%u [label=\"[%u-%u]|%s|savings: %d\", %s, %s];\n",
                node->id, range.start, range.end, escaped_label,
                node->saving_so_far, fillcolor, penwidth);
    }

    // Second pass: Track unique edges
    for (int i = 0; i < node_count; i++) {
        TreeNode *node = &nodes[i];
        if ((!node->isPruned || viz->show_pruned) && node->parent_id != 0) {
            // Check if edge already exists
            bool edge_exists = false;
            for (size_t j = 0; j < viz->edge_count; j++) {
                if (viz->edges[j].parent_id == node->parent_id && 
                    viz->edges[j].child_id == node->id) {
                    edge_exists = true;
                    break;
                }
            }
            
            if (!edge_exists) {
                // Resize edges array if needed
                if (viz->edge_count >= viz->edge_capacity) {
                    viz->edge_capacity = viz->edge_capacity ? viz->edge_capacity * 2 : 16;
                    viz->edges = realloc(viz->edges, viz->edge_capacity * sizeof(Edge));
                }
                
                // Add new edge
                viz->edges[viz->edge_count].parent_id = node->parent_id;
                viz->edges[viz->edge_count].child_id = node->id;
                viz->edge_count++;
                
                // Write edge to file
                fprintf(viz->dot_file, "  node_%u -> node_%u [label=\"w:%d\"];\n",
                        node->parent_id, node->id, node->incoming_weight);
            }
        }
    }
    viz->current_level++;
}

void visualize_mark_pruned(TreeVisualizer* viz, TreeNode* level_nodes, int node_count) {
    if (!viz->dot_file) return;
    
    for (int i = 0; i < node_count; i++) {
        TreeNode* node = &level_nodes[i];
        if (node->isPruned) {
            fprintf(viz->dot_file, "  node_%p [fillcolor=\"#ffdddd\", penwidth=1];\n", (void*)node);
        }
    }
}

void finalize_visualizer(TreeVisualizer* viz) {
    if (viz->dot_file) {
        fprintf(viz->dot_file, "}\n");
        fclose(viz->dot_file);
        viz->dot_file = NULL;
    }
    free(viz->edges);  // Clean up edge tracking
}
/*
// In your main compression code:
#include "tree_visualizer.h"

int main() {
    TreeVisualizer viz;
    init_visualizer(&viz, "compression_tree.dot", true);
    
    // During tree building:
    TreeNode* new_nodes =  ... ;
    int node_count =  ... ;
    TreeNode* old_node =  ... ;
    visualize_add_level(&viz, new_nodes, node_count, old_node, block, block_count);
    
    // After pruning:
    TreeNode* level_nodes =  ... ;
    int pruned_count =  ... ;
    visualize_mark_pruned(&viz, level_nodes, pruned_count);
    
    // When done:
    finalize_visualizer(&viz);
    return 0;
}
*/