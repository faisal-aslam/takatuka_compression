#pragma once
#include <stdint.h>
#include "graph.h"
#include <limits.h>
#include <stdio.h>

#define PATH_CURRENT 0
#define PATH_BEST 1

extern uint32_t max_saving_node_ids[MAX_LEVELS];
extern uint32_t best_savings_node_ids[MAX_LEVELS];

typedef struct {
    uint32_t path_stack[2][MAX_LEVELS];          // node IDs
    uint32_t path_freqs[2][MAX_LEVELS];          // frequencies per node
    uint32_t path_per_node_savings[2][MAX_LEVELS]; // per-node savings
    int32_t  path_size[2];                       // signed: -1 means empty
    uint32_t path_total_saving[2];               // total savings
    uint32_t path_total_cost[2];                 // total cost of the path
    uint32_t path_per_node_cost[2][MAX_LEVELS]; // per-node cost
    uint32_t path_total_freq[2];                 // total frequency
} Path;

extern Path path_state;
extern long total_input_size;

// to find the path with maximum total savings from any leaf to the root node
void find_best_saving_path(const uint8_t* block, uint16_t starting_level);

void free_path_state();

void print_path(uint8_t isCurrent, uint8_t shouldPrintData, const uint8_t *block);

void final_book_keeping(const uint8_t* block);