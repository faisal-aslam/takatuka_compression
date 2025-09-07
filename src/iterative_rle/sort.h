//itertive_rle/sort.h

#pragma once

#include <stdint.h>
#include <stdlib.h>

/* ---- Compact data structure for compression ---- */
typedef struct {
    uint32_t original_size;   // Size of original data
    uint32_t bitmap_bits;     // Number of bits in bitmap
    uint8_t *bitmap;          // Bitmap of comparison results
    // Partially sorted data will be stored separately
} SortHeader;

/* ---- Public API functions ---- */

/**
 * @brief Perform a partial merge sort with limited comparisons
 * 
 * @param data Input data to be partially sorted (will be modified)
 * @param size Size of the input data
 * @param max_comparisons Maximum number of comparisons (0 for full sort)
 * @return SortHeader* Header containing bitmap info (caller must free)
 */
SortHeader* partial_merge_sort(uint8_t *data, uint32_t size, uint32_t max_comparisons);

/**
 * @brief Reconstruct original data from partially sorted data using bitmap
 * 
 * @param sorted_data Partially sorted data
 * @param header Header containing bitmap information
 */
void reconstruct_original(uint8_t *sorted_data, const SortHeader *header);

/**
 * @brief Free memory allocated for SortHeader
 * 
 * @param header Header to free
 */
void free_sort_header(SortHeader *header);

/**
 * @brief Calculate bytes needed for bitmap
 */
uint32_t bitmap_bytes_needed(uint32_t bit_count);
