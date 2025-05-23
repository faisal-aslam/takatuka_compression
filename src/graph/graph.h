// graph/graph.h

#ifndef GRAPH_NODE_H
#define GRAPH_NODE_H

#include "constants.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>


// Cross-platform static assert
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
#define STATIC_ASSERT(cond, msg) typedef char static_assert_##msg[(cond) ? 1 : -1]
#endif

#define GRAPH_MAX_NODES 10000


typedef struct __attribute__((packed)) {
    uint32_t id;                     //4 byte
    uint8_t parent_count;           // 1 byte
    uint8_t child_count;            // 1 byte
    uint8_t parents[SEQ_LENGTH_LIMIT]; //SEQ_LENGTH_LIMIT bytes  
    uint8_t children[SEQ_LENGTH_LIMIT]; //SEQ_LENGTH_LIMIT bytes
    uint8_t incoming_weight;        // 1 byte
    uint32_t compress_start_index;   //4 bytes
    uint8_t compress_sequence;        //1 bytes
    int32_t saving_so_far;          // 4 bytes
    uint32_t level;                 //2 bytes
    // Total: 4+1+1+SEQ_LENGTH_LIMIT+SEQ_LENGTH_LIMIT+1+4+1+4+4 = 20+(2*SEQ_LENGTH_LIMIT) bytes
} GraphNode;

// Verify padding hasn't changed the size
STATIC_ASSERT(sizeof(GraphNode) == 20+(2*SEQ_LENGTH_LIMIT), "GraphNode size mismatch - check compiler padding");
STATIC_ASSERT(SEQ_LENGTH_LIMIT <= 255, "SEQ_LENGTH_LIMIT cannot be greater than 255");
STATIC_ASSERT(SEQ_LENGTH_LIMIT >= 0, "SEQ_LENGTH_LIMIT cannot be less than 0");

void graph_init(void);
GraphNode* graph_get_node(uint32_t index);
bool graph_add_edge(uint32_t from, uint32_t to);
GraphNode* create_new_node();
bool is_graph_full();
void print_graph_node(const GraphNode *node, const uint8_t* block);

uint32_t get_current_graph_node_index();

#endif