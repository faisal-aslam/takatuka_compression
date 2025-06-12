// src/second_pass/prune_logic.c

#include "prune_logic.h"
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // For memcpy
#include <stdlib.h>

#define SELECTION_SORT_THRESHOLD 32


/**
 * Calculates compression savings for a binary sequence based on:
 * - Frequency (higher frequency => higher savings)
 * - Sequence length (longer sequences => much higher savings)
 * - Uniformity (uniform sequences => extra exponential boost)
 *
 * Optimized to avoid pow() for better performance.
 *
 * @param new_bin_seq The binary sequence to evaluate
 * @param seq_length Length of the sequence
 * @param map Hashmap containing frequency counts
 * @return Calculated savings value, or INT_MIN on error
 */
int32_t calculate_savings(const uint8_t* new_bin_seq, uint16_t seq_length, BinSeqMap* map) {
    // Validate inputs
    if (!new_bin_seq || seq_length <= 0) {
        fprintf(stderr, "Error: Invalid parameters in calculate_savings\n");
        return INT_MIN;
    }

    // No compression gain possible from sequences of length 1
    if (seq_length == 1) {
        return 0;
    }

    // Lookup frequency of the sequence from the hashmap
    //const int* freq_ptr = map == NULL? 0: binseq_map_get_frequency(map, new_bin_seq, seq_length);
    int frequency = 0;//freq_ptr ? *freq_ptr : 0;
    if (frequency == 0) {
        return seq_length*5;
    }
    double savings = (double)(pow(frequency+1, 1.8))*(pow(seq_length, 1.3));
    
    /**
     * Safety Cap:
     * - Ensures theoretical limits are respected
     */
    return (int32_t)MIN(savings, INT32_MAX);
}

