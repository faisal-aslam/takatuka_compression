#pragma once
#include <stdint.h>
#include "graph.h"
#include <limits.h>
#include <stdio.h>

#define PATH_CURRENT 0
#define PATH_BEST 1

typedef struct {
    uint32_t path_stack[2][MAX_LEVELS];   // 0 = current, 1 = best
    uint32_t path_freqs[2][MAX_LEVELS];   // frequencies per node
    double path_per_node_savings[2][MAX_LEVELS];  // per-node cost
    uint32_t path_size[2];                 // size of each path
    double path_total_saving[2];                  // total cost
    uint32_t path_total_freq[2];
} Path;

extern Path path_state;
extern long total_input_size;

// to find the path with maximum total savings from any leaf to the root node
void find_best_saving_path(const uint8_t* block, uint16_t starting_level);

void free_path_state();

void print_path(uint8_t isCurrent, uint8_t shouldPrintData, const uint8_t *block);

