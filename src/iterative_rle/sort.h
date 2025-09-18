// iterative_rle/sort.h
#pragma once

#include <stdint.h>
#include <stddef.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

typedef struct {
    size_t original_size;     // size of original array
    size_t bitmap_size;       // number of valid entries in bitmap
    uint8_t *bitmap;          // merge decisions (0=left, 1=right) - dynamically allocated
    size_t bitmap_capacity;   // capacity of bitmap array
    int max_block_size;       // max block size allowed to merge
} SortInfo;

extern SortInfo sort_info;  // global instance

/* Sorting API */
void merge_sort(uint8_t arr[], int n, int max_block_size);
void merge_sort_reverse(uint8_t arr[], int n);
