#ifndef SHORTEST_PATH_H
#define SHORTEST_PATH_H

#include "graph.h"
#include <stdbool.h>

typedef struct {
    uint32_t node_id;
    uint64_t seen_sequences;
    int cost;
    uint32_t* path;       // Array of node IDs in the path
    uint32_t path_length; // Current length of the path
} PathState;

// Finds shortest path from any source node to sink (node_id = 0)
int find_shortest_path_to_sink(uint32_t** path, uint32_t* path_length);

#endif