#pragma once

#include <stdint.h>
#include "graph.h"
#include <limits.h>
#include <stdio.h>

typedef struct {
    // Core path data (direct pointers to internal arrays)
    const uint32_t* nodes;          // Node IDs 
    const uint32_t* freqs;          // Node frequencies  

    // Metadata
    int32_t path_size;              // These are the total number of nodes. 
} BestPathView;

extern BestPathView view;

/**
 * @brief Sets the global BestPathView used by compression.
 *
 * Every shortest path algorithm variant must call this function after it finishes.
 * The compression stage relies on the view set here to write the compressed output.
 *
 * Important:
 *  - The arrays passed (path_ids, path_frequencies) must remain valid for the lifetime
 *    of 'view' (e.g. allocated on the heap or managed globally).
 *    Do NOT pass pointers to temporary stack arrays.
 *  - path_size must be non-negative and reflect the number of nodes in the path.
 */
void set_best_path_view(uint32_t *path_ids, uint32_t *path_frequencies, int32_t path_size);

void print_best_view(const BestPathView *view, uint8_t shouldPrintData, const uint8_t *block);

