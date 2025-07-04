// shortest_path.h
#ifndef SHORTEST_PATH_H
#define SHORTEST_PATH_H
#include <stdint.h>
#include "graph.h"
#include "../map/sequence_repository_freq.h"
#include <limits.h>
#include <stdio.h>

extern long total_input_size;
extern SequenceRepository useless_repo;
// Finds and prints the shortest path from any last-level node to root (node_id = 0)
void find_shortest_path_to_sink(const uint8_t* block);

#endif
