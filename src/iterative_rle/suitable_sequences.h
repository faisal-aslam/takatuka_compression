#pragma once
#include <stdint.h>

int find_suitable_subblock(uint8_t *arr, int n, double density,
                           int min_size, int *best_start, int *best_end);