// graph_node.h
#ifndef GRAPH_NODE_H
#define GRAPH_NODE_H

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
#define GRAPH_MAX_CONNECTIONS 32

typedef struct __attribute__((packed)) {
    uint32_t id;                    // 4 bytes
    uint8_t parent_count;           // 1 byte
    uint8_t child_count;            // 1 byte
    uint8_t parents[GRAPH_MAX_CONNECTIONS];  // 32 bytes
    uint8_t children[GRAPH_MAX_CONNECTIONS]; // 32 bytes
    uint8_t incoming_weight;        // 1 byte
    uint32_t sequence_range;        // 4 bytes (packed start/end)
    int32_t saving_so_far;          // 4 bytes
    // Total: 4+1+1+32+32+1+4+4 = 79 bytes
} GraphNode;

// Verify padding hasn't changed the size
STATIC_ASSERT(sizeof(GraphNode) == 79, "GraphNode size mismatch - check compiler padding");

#endif