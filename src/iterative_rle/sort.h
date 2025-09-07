//itertive_rle/sort.h

#pragma once

#include <stdint.h>
#include <stdlib.h>

/* ---- Minimal data structure ---- 
* we must not store anything but bitmap as 
* compression needs to store least amount of data for 
* it to work.
*/
typedef struct {
    uint32_t original_size;
    uint32_t bitmap_bits;
    uint8_t *bitmap;
} SortHeader;

/* ---- Public API ---- */
SortHeader* partial_merge_sort(uint8_t *data, uint32_t size, uint32_t max_comparisons);
void reconstruct_original(uint8_t *sorted_data, const SortHeader *header);
void free_sort_header(SortHeader *header);
uint32_t bitmap_bytes_needed(uint32_t bit_count);

