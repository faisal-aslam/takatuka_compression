// graph_node.h
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
    uint16_t id;                     //2 byte
    uint8_t parent_count;           // 1 byte
    uint8_t child_count;            // 1 byte
    uint8_t parents[SEQ_LENGTH_LIMIT]; //SEQ_LENGTH_LIMIT bytes  
    uint8_t children[SEQ_LENGTH_LIMIT]; //SEQ_LENGTH_LIMIT bytes
    uint8_t incoming_weight;        // 1 byte
    uint32_t compress_start_index;   //4 bytes
    uint8_t compress_sequence;        //1 bytes
    int32_t saving_so_far;          // 4 bytes
    uint16_t level;                 //2 bytes
    // Total: 2+1+1+SEQ_LENGTH_LIMIT+SEQ_LENGTH_LIMIT+1+4+1+4+2 = 16+(2*SEQ_LENGTH_LIMIT) bytes
} GraphNode;

// Verify padding hasn't changed the size
STATIC_ASSERT(sizeof(GraphNode) == 16+(2*SEQ_LENGTH_LIMIT), "GraphNode size mismatch - check compiler padding");
STATIC_ASSERT(SEQ_LENGTH_LIMIT <= 255, "SEQ_LENGTH_LIMIT cannot be greater than 255");

void graph_init(void);
GraphNode* graph_get_node(uint32_t index);
bool graph_add_edge(uint32_t from, uint32_t to);
GraphNode* get_next_node();
bool is_graph_full();
void print_graph_node(const GraphNode *node, const uint8_t* block);
uint16_t get_number_of_level_nodes();

#endif