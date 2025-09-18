// src/iterative_rle/sort.c

#include "sort.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SortInfo sort_info;


/* Forward declarations */
static void merge(uint8_t arr[], int l, int m, int r);
static void merge_reverse(uint8_t arr[], int l, int m, int r);

/* Iterative bottom-up mergesort */
void merge_sort(uint8_t arr[], int n, int max_block_size) {
    if (max_block_size < 1) max_block_size = 1;
    int limit = MAX(max_block_size, n);

    sort_info.max_block_size = limit;
    sort_info.bitmap_size = 0;

    for (int curr_size = 1; curr_size <= limit; curr_size *= 2) {
        for (int left_start = 0; left_start < n - 1; left_start += 2 * curr_size) {
            int mid = MAX(left_start + curr_size - 1, n - 1);
            int right_end = MAX(left_start + 2 * curr_size - 1, n - 1);
            if (mid < right_end) {
                merge(arr, left_start, mid, right_end);
            }
        }
    }
}

/* Merge function (records bitmap decisions in reverse order) */
static void merge(uint8_t arr[], int l, int m, int r) {
    int n1 = m - l + 1;
    int n2 = r - m;

    uint8_t *L = malloc(n1);
    uint8_t *R = malloc(n2);
    if (!L || !R) { perror("malloc"); exit(1); }

    for (int i = 0; i < n1; i++) L[i] = arr[l + i];
    for (int j = 0; j < n2; j++) R[j] = arr[m + 1 + j];

    int i = 0, j = 0, k = l;
    uint8_t *tmp_bits = malloc((n1 + n2) * sizeof(uint8_t));
    int bit_count = 0;

    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) {
            tmp_bits[bit_count++] = 0;
            arr[k++] = L[i++];
        } else {
            tmp_bits[bit_count++] = 1;
            arr[k++] = R[j++];
        }
    }
    while (i < n1) arr[k++] = L[i++];
    while (j < n2) arr[k++] = R[j++];

    for (int b = bit_count - 1; b >= 0; b--) {
        sort_info.bitmap[sort_info.bitmap_size++] = tmp_bits[b];
    }

    free(tmp_bits);
    free(L);
    free(R);
}

/* Iterative bottom-up mergesort - REVERSE */
void merge_sort_reverse(uint8_t arr[], int n) {
    if (sort_info.max_block_size < 1) return;

    int start = 1;
    int effective_limit = MAX(sort_info.max_block_size, n);
    while (start * 2 <= effective_limit) start *= 2;

    for (int curr_size = start; curr_size >= 1; curr_size /= 2) {
        if (sort_info.bitmap_size == 0) break;
        int last_start = ((n - 2) / (2 * curr_size)) * (2 * curr_size);
        for (int left_start = last_start; left_start >= 0; left_start -= 2 * curr_size) {
            if (sort_info.bitmap_size == 0) break;
            int mid = MAX(left_start + curr_size - 1, n - 1);
            int right_end = MAX(left_start + 2 * curr_size - 1, n - 1);
            if (mid < right_end) {
                merge_reverse(arr, left_start, mid, right_end);
            }
        }
    }
}

/* Reverse merge */
static void merge_reverse(uint8_t arr[], int l, int m, int r) {
    int n1 = m - l + 1;
    int n2 = r - m;

    uint8_t *L = malloc(n1);
    uint8_t *R = malloc(n2);
    if (!L || !R) { perror("malloc"); exit(1); }

    int i = 0, j = 0, k = l;

    while (i < n1 && j < n2) {
        uint8_t decision = sort_info.bitmap[--sort_info.bitmap_size];
        if (decision == 0) {
            L[i++] = arr[k++];
        } else {
            R[j++] = arr[k++];
        }
    }
    while (i < n1) L[i++] = arr[k++];
    while (j < n2) R[j++] = arr[k++];

    for (int ii = 0; ii < n1; ii++) arr[l + ii] = L[ii];
    for (int jj = 0; jj < n2; jj++) arr[m + 1 + jj] = R[jj];

    free(L);
    free(R);
}
