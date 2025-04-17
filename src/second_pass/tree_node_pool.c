//tree_node_pool.c
#include "tree_node_pool.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tree_node.h"

#define INITIAL_POOL_CAPACITY 1024

void init_tree_node_pool_manager(TreeNodePoolManager* mgr) {
    mgr->active_index = 0;
    for (int i = 0; i < 2; i++) {
        mgr->pool[i].data = NULL;
        mgr->pool[i].size = 0;
        mgr->pool[i].capacity = 0;
    }
}



TreeNode* alloc_tree_node(TreeNodePoolManager* mgr) {
    TreeNodePool* pool = get_active_pool(mgr);

    if (pool->size >= pool->capacity) {
        size_t new_capacity = pool->capacity ? pool->capacity * 2 : INITIAL_POOL_CAPACITY;
        TreeNode* new_data = (TreeNode*)malloc(new_capacity * sizeof(TreeNode));
        if (!new_data) {
            fprintf(stderr, "Failed to allocate memory for TreeNodePool\n");
            exit(1);
        }

        if (pool->data) {
            memcpy(new_data, pool->data, pool->size * sizeof(TreeNode));
            free(pool->data);
        }

        pool->data = new_data;
        pool->capacity = new_capacity;
    }

    return &pool->data[pool->size++];
}

void switch_tree_node_pool(TreeNodePoolManager* mgr) {
    // Switch active pool index
    mgr->active_index ^= 1;

    // Reset the new pool
    TreeNodePool* new_pool = get_active_pool(mgr);
    new_pool->size = 0;

    // Optionally free memory if needed
    // If you want to reclaim memory aggressively, uncomment:
    // if (new_pool->data) {
    //     free(new_pool->data);
    //     new_pool->data = NULL;
    //     new_pool->capacity = 0;
    // }
}

void free_tree_node_pool_manager(TreeNodePoolManager* mgr) {
    for (int i = 0; i < 2; i++) {
        free(mgr->pool[i].data);
        mgr->pool[i].data = NULL;
        mgr->pool[i].size = 0;
        mgr->pool[i].capacity = 0;
    }
    mgr->active_index = 0;
}

size_t get_current_pool_size(TreeNodePoolManager* mgr) {
    return mgr->pool[mgr->active_index].size;
}

size_t get_current_pool_capacity(TreeNodePoolManager* mgr) {
    return mgr->pool[mgr->active_index].capacity;
}

uint8_t is_valid_pool_index(TreeNodePoolManager* mgr, size_t index) {
    return index < mgr->pool[mgr->active_index].size;
}

TreeNodePool* get_active_pool(TreeNodePoolManager* mgr) {
    return &mgr->pool[mgr->active_index];
}

TreeNodePool* get_inactive_pool(TreeNodePoolManager* mgr) {
    return &mgr->pool[mgr->active_index ^ 1];
}