// shortest_path_A_star.c

#include "shortest_path.h"
#include "graph.h"
#include "map/sequence_repository_freq.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#define MAX_QUEUE_SIZE MAX_GRAPH_NODES

// Cost calculation remains the same
#define COST(len, freq) (((len) == 1) ? 1 : (((freq) == 1) ? ((len) + 1) : 1))

typedef struct {
    uint32_t node_id;
    int32_t g_cost;  // Actual cost from start to current node
    int32_t f_cost;  // Estimated total cost (g + h)
    int32_t path_size;
} AStarNode;

typedef struct {
    uint32_t node_id;
    uint32_t parent_id;
    int32_t cost;
    int32_t path_size;
} PathNode;

static PathNode path_nodes[MAX_GRAPH_NODES];
static bool visited[MAX_GRAPH_NODES] = {false};
static int32_t best_path_cost = INT32_MAX;
static int32_t best_path_size = -1;
static uint32_t best_path_stack[MAX_GRAPH_NODES];
static int best_count = 0;

// Priority queue implementation for A*
typedef struct {
    AStarNode *nodes;
    int capacity;
    int size;
} PriorityQueue;

static PriorityQueue* pq_create(int capacity) {
    PriorityQueue *pq = (PriorityQueue*)malloc(sizeof(PriorityQueue));
    pq->nodes = (AStarNode*)malloc(capacity * sizeof(AStarNode));
    pq->capacity = capacity;
    pq->size = 0;
    return pq;
}

static void pq_free(PriorityQueue *pq) {
    free(pq->nodes);
    free(pq);
}

static void pq_push(PriorityQueue *pq, AStarNode node) {
    if (pq->size >= pq->capacity) {
        // In production, you might want to grow the queue here
        assert(!"Priority queue overflow");
        return;
    }
    
    int i = pq->size++;
    pq->nodes[i] = node;
    
    // Bubble up
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (pq->nodes[i].f_cost >= pq->nodes[parent].f_cost) break;
        AStarNode temp = pq->nodes[i];
        pq->nodes[i] = pq->nodes[parent];
        pq->nodes[parent] = temp;
        i = parent;
    }
}

static AStarNode pq_pop(PriorityQueue *pq) {
    AStarNode result = pq->nodes[0];
    pq->nodes[0] = pq->nodes[--pq->size];
    
    // Bubble down
    int i = 0;
    while (1) {
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        int smallest = i;
        
        if (left < pq->size && pq->nodes[left].f_cost < pq->nodes[smallest].f_cost)
            smallest = left;
        if (right < pq->size && pq->nodes[right].f_cost < pq->nodes[smallest].f_cost)
            smallest = right;
            
        if (smallest == i) break;
        
        AStarNode temp = pq->nodes[i];
        pq->nodes[i] = pq->nodes[smallest];
        pq->nodes[smallest] = temp;
        i = smallest;
    }
    
    return result;
}

// Heuristic function - estimate cost from current node to goal
static inline uint16_t get_node_level(uint32_t node_id) {
    GraphNode *node = get_graph_node(node_id);
    return node ? node->node_level : 0;
}

static int32_t heuristic(uint32_t node_id) {
    // For DAGs with levels, we can use level difference as heuristic
    // This is admissible since it never overestimates the actual cost
    return get_node_level(node_id);
}

static void reconstruct_path(uint32_t current_node, const uint8_t* block) {
    // First reset frequencies since we're calculating fresh
    seq_repo_reset();
    
    // Reconstruct path from current_node to start
    int32_t path_size = 0;
    int32_t total_cost = 0;
    uint32_t temp_path[MAX_GRAPH_NODES];
    
    // Walk backwards from current node to root
    uint32_t node = current_node;
    while (1) {
        temp_path[path_size++] = node;
        if (node == 0) break; // Reached root
        node = path_nodes[node].parent_id;
    }
    
    // Reverse the path to get correct order (root to leaf)
    uint32_t final_path[MAX_GRAPH_NODES];
    for (int i = 0; i < path_size; i++) {
        final_path[i] = temp_path[path_size - 1 - i];
    }
    
    // Calculate actual cost with proper frequencies
    for (int i = 0; i < path_size; i++) {
        GraphNode *node = get_graph_node(final_path[i]);
        if (!node) continue;
        
        uint32_t freq = 1;
        if (node->sequence_length > 1) {
            freq = seq_repo_increase_frequency(&block[node->offset], node->sequence_length);
        }
        int32_t added_cost = COST(node->sequence_length, freq);
        if (node->node_id == 0) added_cost = 0;
        total_cost += added_cost;
    }
    
    // Update if this is the best path found so far
    if (total_cost < best_path_cost || 
        (total_cost == best_path_cost && path_size < best_path_size)) {
        best_path_cost = total_cost;
        best_path_size = path_size;
        memcpy(best_path_stack, final_path, path_size * sizeof(uint32_t));
        best_count++;
        printf(" saved the path %d with cost: %d\n", best_count, total_cost);
    }
}

static void print_path(const uint8_t* block) {
    printf(" Shortest path size=%d, cost=%d \n", best_path_size, best_path_cost);
    for (int32_t i = 0; i < best_path_size; i++) {
        printf("%u", best_path_stack[i]);
        if (i < best_path_size - 1) {
            printf(" -> ");
        }
    }
    printf("\n");
    
    for (int32_t i = 0; i < best_path_size; i++) {
        uint32_t node_id = best_path_stack[i];
        GraphNode *node = get_graph_node(node_id);
        if (!node) continue;
        if (i > 0) printf(" -> ");
        print_node_sequence(node, block);
    }
    printf("\n");
}

void find_shortest_path_A_star(const uint8_t *block) {
    PriorityQueue *open_set = pq_create(MAX_QUEUE_SIZE);
    best_path_cost = INT32_MAX;
    best_path_size = -1;
    best_count = 0;
    
    // Initialize path nodes and visited array
    for (uint32_t i = 0; i < MAX_GRAPH_NODES; i++) {
        path_nodes[i].cost = INT32_MAX;
        path_nodes[i].path_size = INT32_MAX;
        visited[i] = false;
    }
    
    uint16_t last_level = get_last_level_index();
    uint32_t start = get_level_start_id(last_level);
    uint32_t end = get_level_end_id(last_level);
    
    // Initialize open set with all leaf nodes
    for (uint32_t i = start; i < end; i++) {
        GraphNode *node = get_graph_node(i);
        if (node && !node->isUseless) {
            // For initial cost estimation, we don't update frequencies permanently
            uint32_t freq = node->sequence_length > 1 ? 
                seq_repo_get_frequency(&block[node->offset], node->sequence_length) : 1;
            int32_t added_cost = COST(node->sequence_length, freq);
            if (node->node_id == 0) added_cost = 0;
            
            path_nodes[i].node_id = i;
            path_nodes[i].parent_id = UINT32_MAX;
            path_nodes[i].cost = added_cost;
            path_nodes[i].path_size = 1;
            visited[i] = true;
            
            AStarNode astar_node = {
                .node_id = i,
                .g_cost = added_cost,
                .f_cost = added_cost + heuristic(i),
                .path_size = 1
            };
            pq_push(open_set, astar_node);
        }
    }
    
    while (open_set->size > 0) {
        AStarNode current = pq_pop(open_set);
        
        // If we've found a better path to this node since it was added to the queue, skip it
        if (current.g_cost > path_nodes[current.node_id].cost) {
            continue;
        }
        
        // If this is the root node, reconstruct and evaluate the path
        if (current.node_id == 0) {
            reconstruct_path(current.node_id, block);
            continue;
        }
        
        // Get parent nodes
        GraphNode *node = get_graph_node(current.node_id);
        uint8_t parent_count = get_parent_nodes_count(node);
        GraphNode *parents = get_parent_nodes(node);
        
        for (uint8_t i = 0; i < parent_count; i++) {
            GraphNode *parent = &parents[i];
            if (parent->isUseless) continue;
            
            uint32_t parent_id = parent->node_id;
            
            // Calculate tentative cost (without permanently updating frequency)
            uint32_t freq = parent->sequence_length > 1 ? 
                seq_repo_get_frequency(&block[parent->offset], parent->sequence_length) : 1;
            int32_t added_cost = COST(parent->sequence_length, freq);
            if (parent_id == 0) added_cost = 0;
            
            int32_t tentative_g_cost = current.g_cost + added_cost;
            
            // If we found a better path to the parent
            if (!visited[parent_id] || tentative_g_cost < path_nodes[parent_id].cost) {
                visited[parent_id] = true;
                path_nodes[parent_id].node_id = parent_id;
                path_nodes[parent_id].parent_id = current.node_id;
                path_nodes[parent_id].cost = tentative_g_cost;
                path_nodes[parent_id].path_size = current.path_size + 1;
                
                AStarNode neighbor_node = {
                    .node_id = parent_id,
                    .g_cost = tentative_g_cost,
                    .f_cost = tentative_g_cost + heuristic(parent_id),
                    .path_size = current.path_size + 1
                };
                pq_push(open_set, neighbor_node);
            }
        }
    }
    
    pq_free(open_set);
    print_path(block);
}