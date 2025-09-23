// src/iterative_rle/sort.c

#include "sort.h"
#include <math.h> // for log2, floor
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SortInfo sort_info = {0, 0, NULL, 0, 0}; // ensure zeros/NULL initialised

/* Forward declarations */
static void merge(uint8_t arr[], int l, int m, int r);
static void merge_reverse(uint8_t arr[], int l, int m, int r);

static inline void print_array(const uint8_t *arr, int size) {
    for (int i = 0; i < size; i++) {
        printf("%u ", arr[i]);
    }
    printf("\n");
}

// Compute max bitmap size based on n and limit
static inline size_t calc_bitmap_capacity(size_t n, size_t limit) {
    if (limit < 1) limit = 1; // safety
    size_t passes = 1 + (size_t)floor(log2((double)limit));
    return n * passes;
}

/* Helper: ensure bitmap has capacity 'cap' */
static int ensure_bitmap_capacity(size_t cap) {
    if (sort_info.bitmap_capacity >= cap) return 0;
    size_t newcap = sort_info.bitmap_capacity ? sort_info.bitmap_capacity : 1;
    while (newcap < cap)
        newcap *= 2;
    uint8_t *nb = realloc(sort_info.bitmap, newcap * sizeof(uint8_t));
    if (!nb) return -1;
    sort_info.bitmap = nb;
    sort_info.bitmap_capacity = newcap;
    return 0;
}

/* Iterative bottom-up mergesort */
void merge_sort(uint8_t arr[], int n, int max_block_size) {
    if (max_block_size < 1) max_block_size = 1;
    int limit = MIN(max_block_size, n);
#ifdef DEBUG1
    printf("input array of size=%d\n", n);
    print_array(arr, n);
#endif
    sort_info.max_block_size = limit;
    sort_info.bitmap_size = 0;

    /* Reserve a bitmap large enough for all passes.
    Each pass uses at most n bits, and there are 1+floor(log2(limit)) passes. */
    size_t needed = calc_bitmap_capacity((size_t)n, (size_t)limit);
    if (needed == 0) needed = 1;
    if (ensure_bitmap_capacity(needed) != 0) {
        perror("realloc");
        exit(1);
    }

    for (int curr_size = 1; curr_size <= limit; curr_size *= 2) {
#ifdef DEBUG1
        printf("\n\n************************** current merge size=%d\n\n", curr_size);
#endif        
        for (int left_start = 0; left_start < n - 1; left_start += 2 * curr_size) {
            int mid = MIN(left_start + curr_size - 1, n - 1);
            int right_end = MIN(left_start + 2 * curr_size - 1, n - 1);
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

    uint8_t *L = malloc((size_t)n1 * sizeof(uint8_t));
    uint8_t *R = malloc((size_t)n2 * sizeof(uint8_t));
    if (!L || !R) {
        perror("malloc");
        exit(1);
    }

    for (int i = 0; i < n1; i++)
        L[i] = arr[l + i];
    for (int j = 0; j < n2; j++)
        R[j] = arr[m + 1 + j];

#ifdef DEBUG1
    printf("l=%d, m=%d, r=%d\n", l, m, r);
    print_array(L, n1);
    print_array(R, n2);
#endif

    int i = 0, j = 0, k = l;
    uint8_t *tmp_bits = malloc((size_t)(n1 + n2) * sizeof(uint8_t));
    if (!tmp_bits) {
        perror("malloc");
        free(L);
        free(R);
        exit(1);
    }
    int bit_count = 0;

    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) {
            tmp_bits[bit_count++] = 0;
#ifdef DEBUG1
            printf("bit=0, bit_count=%ld\n", sort_info.bitmap_size+bit_count);
#endif
            arr[k++] = L[i++];
        } else {
            tmp_bits[bit_count++] = 1;
#ifdef DEBUG1
            printf("bit=1, bit_count=%ld\n", sort_info.bitmap_size+bit_count);
#endif

            arr[k++] = R[j++];
        }
    }
    while (i < n1)
        arr[k++] = L[i++];
    while (j < n2)
        arr[k++] = R[j++];

    /* Ensure bitmap has space for the new bits */
    if (ensure_bitmap_capacity(sort_info.bitmap_size + (size_t)bit_count) != 0) {
        perror("realloc");
        free(tmp_bits);
        free(L);
        free(R);
        exit(1);
    }

    /* Store decisions in reverse order as before */
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
    int effective_limit = MIN(sort_info.max_block_size, n);
    while (start * 2 <= effective_limit)
        start *= 2;

    for (int curr_size = start; curr_size >= 1; curr_size /= 2) {
        if (sort_info.bitmap_size == 0) break;
        int last_start = ((n - 2) / (2 * curr_size)) * (2 * curr_size);
        for (int left_start = last_start; left_start >= 0; left_start -= 2 * curr_size) {
            if (sort_info.bitmap_size == 0) break;
            int mid = MIN(left_start + curr_size - 1, n - 1);
            int right_end = MIN(left_start + 2 * curr_size - 1, n - 1);
            if (mid < right_end) {
                merge_reverse(arr, left_start, mid, right_end);
            }
        }
    }

    /* finished using bitmap: free it to avoid "still reachable" blocks */
    if (sort_info.bitmap) {
        free(sort_info.bitmap);
        sort_info.bitmap = NULL;
        sort_info.bitmap_capacity = 0;
        sort_info.bitmap_size = 0;
    }
}

/* Reverse merge */
static void merge_reverse(uint8_t arr[], int l, int m, int r) {
    int n1 = m - l + 1;
    int n2 = r - m;

    uint8_t *L = malloc((size_t)n1 * sizeof(uint8_t));
    uint8_t *R = malloc((size_t)n2 * sizeof(uint8_t));
    if (!L || !R) {
        perror("malloc");
        exit(1);
    }

    int i = 0, j = 0, k = l;

    while (i < n1 && j < n2) {
        if (sort_info.bitmap_size == 0) {
            /* Shouldn't happen unless bitmap is corrupted; guard to avoid underflow */
            fprintf(stderr, "merge_reverse: bitmap underflow\n");
            free(L);
            free(R);
            exit(1);
        }
        uint8_t decision = sort_info.bitmap[--sort_info.bitmap_size];
        if (decision == 0) {
            L[i++] = arr[k++];
        } else {
            R[j++] = arr[k++];
        }
    }
    while (i < n1)
        L[i++] = arr[k++];
    while (j < n2)
        R[j++] = arr[k++];

    for (int ii = 0; ii < n1; ii++)
        arr[l + ii] = L[ii];
    for (int jj = 0; jj < n2; jj++)
        arr[m + 1 + jj] = R[jj];

    free(L);
    free(R);
}
