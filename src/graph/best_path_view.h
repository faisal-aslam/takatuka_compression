#ifndef BEST_VIEW_H
#define BEST_VIEW_H

#include <stdint.h>
#include "graph.h"
#include <limits.h>
#include <stdio.h>

typedef struct {
    // Core path data (direct pointers to internal arrays)
    const uint32_t* nodes;          // Node IDs [path_size elements]
    const int32_t* costs;           // Node costs [path_size elements]
    const uint32_t* freqs;          // Node frequencies [path_size elements]
    const double* per_node_costs;   // Per-node costs [path_size elements]
    
    // Code fields (to be populated later)
    uint16_t* per_node_codes;       // Will be allocated by writer
    uint8_t* codes_length;          // Will be allocated by writer
    
    // Metadata
    int32_t path_size;              // From path_state.path_size[PATH_BEST]
    double total_cost;              // From path_state.path_total_cost[PATH_BEST]
} BestPathView;

BestPathView get_best_path_view();

void print_best_view(const BestPathView *view, uint8_t shouldPrintData, const uint8_t *block);

#endif