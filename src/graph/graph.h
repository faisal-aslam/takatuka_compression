// graph/graph.h

#ifndef GRAPH_NODE_H
#define GRAPH_NODE_H

#include "../constants.h"
#include <stdint.h>
#include <stdbool.h>

#define GRAPH_MAX_NODES 10000
#define MAX_WEIGHT SEQ_LENGTH_LIMIT
#define MAX_LEVELS 1024
#define OVERFLOW_SLOT_CAPACITY 8192

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
#define STATIC_ASSERT(cond, msg) typedef char static_assert_##msg[(cond) ? 1 : -1]
#endif

typedef struct __attribute__((packed)) {
    uint32_t id;
    uint8_t parent_count;
    uint8_t child_count;
    uint8_t parents[SEQ_LENGTH_LIMIT];
    uint8_t children[SEQ_LENGTH_LIMIT];
    uint8_t incoming_weight;
    uint32_t compress_start_index;
    uint8_t compress_sequence;
    int32_t saving_so_far;
    uint32_t level;
} GraphNode;

typedef struct {
    uint32_t indices[SEQ_LENGTH_LIMIT];
    uint8_t count;
} WeightLevelSlot;

typedef struct {
    uint32_t indices[OVERFLOW_SLOT_CAPACITY];
    uint32_t count;
} WeightOverflowSlot;

typedef struct {
    WeightLevelSlot slots[MAX_WEIGHT][MAX_LEVELS];
    uint32_t max_level;
} GraphIndex;

typedef struct {
    GraphNode nodes[GRAPH_MAX_NODES];
    GraphIndex index;
    WeightOverflowSlot overflow_slots[MAX_WEIGHT];
    uint32_t current_node_index;
    bool initialized;
} Graph;

STATIC_ASSERT(sizeof(GraphNode) == 20 + (2 * SEQ_LENGTH_LIMIT), "GraphNode size mismatch");
STATIC_ASSERT(SEQ_LENGTH_LIMIT <= 255, "SEQ_LENGTH_LIMIT too large");
STATIC_ASSERT(SEQ_LENGTH_LIMIT > 0, "SEQ_LENGTH_LIMIT too small");


extern Graph graph;

void graph_init(void);
GraphNode* graph_get_node(uint32_t index);
bool graph_add_edge(uint32_t from, uint32_t to);
GraphNode* create_new_node(uint8_t weight, uint32_t level);
bool is_graph_full(void);
void print_graph_node(const GraphNode *node, const uint8_t* block);
uint32_t get_current_graph_node_index(void);
const uint32_t* get_nodes_by_weight_and_level(uint8_t weight, uint32_t level, uint32_t* count);
const uint32_t* get_overflow_nodes_by_weight(uint8_t weight, uint32_t* count);
uint32_t get_max_level(void);

#endif
