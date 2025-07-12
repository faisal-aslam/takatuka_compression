// shortest_path.h
#ifndef SHORTEST_PATH_H
#define SHORTEST_PATH_H
#include <stdint.h>
#include "graph.h"
#include "../map/sequence_repository_freq.h"
#include <limits.h>
#include <stdio.h>

#define PATH_CURRENT 0
#define PATH_BEST 1

typedef struct {
    uint32_t path_stack[2][MAX_LEVELS];   // 0 = current, 1 = best
    int32_t cost_stack[2][MAX_LEVELS];    // cost per node
    uint32_t path_freqs[2][MAX_LEVELS];   // frequencies per node
    double path_per_node_costs[2][MAX_LEVELS];     // per-node cost
    int32_t path_size[2];                 // size of each path
    double path_total_cost[2];                  // total cost
    uint32_t freq_eq_1_count; //only for the current path. A guard not to copy a bad path to current path.
    uint16_t code_per_node[MAX_LEVELS]; //the code assigned to the best path.
} Path;

extern Path path_state;
extern long total_input_size;
extern SequenceRepository useless_repo;
// Finds and prints the shortest path from any last-level node to root (node_id = 0)
void find_shortest_path_to_sink(const uint8_t* block);

void free_path_state();

#endif
