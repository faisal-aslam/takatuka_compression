#ifndef TREE_NODE_POOL_H
#define TREE_NODE_POOL_H

#include <stddef.h> // for size_t
#include <stdint.h>

typedef struct TreeNode TreeNode;


typedef struct {
    TreeNode* data;
    size_t size;
    size_t capacity;
} TreeNodePool;

typedef struct {
    TreeNodePool pool[2];
    int active_index;
} TreeNodePoolManager;

// Initialize manager (zeroes everything)
void init_tree_node_pool_manager(TreeNodePoolManager* mgr);

// Allocates a new TreeNode from the current active pool
TreeNode* alloc_tree_node(TreeNodePoolManager* mgr);

// Switches to the other pool and clears it (frees memory or keeps for reuse)
void switch_tree_node_pool(TreeNodePoolManager* mgr);

// Frees both pools and resets everything
void free_tree_node_pool_manager(TreeNodePoolManager* mgr);

// Get current pool size
size_t get_current_pool_size(TreeNodePoolManager* mgr);

// Get current pool capacity
size_t get_current_pool_capacity(TreeNodePoolManager* mgr);

// Check if index is valid
uint8_t is_valid_pool_index(TreeNodePoolManager* mgr, size_t index);

// Get current active pool
TreeNodePool* get_active_pool(TreeNodePoolManager* mgr);

// Get inactive pool
TreeNodePool* get_inactive_pool(TreeNodePoolManager* mgr);

#endif

