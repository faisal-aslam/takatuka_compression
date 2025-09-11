// iterative_rle/sort.h

#pragma once

#include <stdint.h>
#include <stddef.h>  // for size_t

/*
 * ---- Minimal data structure ----
 * Only bitmap and essential size metadata are stored
 * to keep memory overhead minimal for compression.
 */
typedef struct {
    size_t original_size;          // size of original array
    size_t bitmap_size;            // number of valid entries in bitmap
    uint8_t bitmap[65000];         // merge decisions (0=right, 1=left)
    int max_block_size;             //max block size allowed to merge.
} SortInfo;


extern SortInfo sort_info;         // global instance
