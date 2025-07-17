#ifndef BEST_VIEW_H
#define BEST_VIEW_H

#include <stdint.h>
#include "../graph/graph.h"
#include <limits.h>
#include <stdio.h>

typedef struct {
    // Core path data (direct pointers to internal arrays)
    const uint32_t* nodes;          // Node IDs 
    const uint32_t* freqs;          // Node frequencies  

    // Metadata
    int32_t path_size;              // These are the total number of nodes. 
} BestPathView;

BestPathView get_best_path_view();

void print_best_view(const BestPathView *view, uint8_t shouldPrintData, const uint8_t *block);

#endif