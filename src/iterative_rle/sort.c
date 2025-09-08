// src/iterative_rle/sort.c

// src/iterative_rle/sort.c
#include "sort.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SortInfo sort_info;

uint32_t bitmap_used = 0;
/* Utility */
static inline int min_int(int x, int y) { return (x < y) ? x : y; }

typedef enum { MERGE_FORWARD = 0, MERGE_REVERSE = 1 } merge_mode_t;

/* Forward declarations */
static void merge(int arr[], int l, int m, int r, merge_mode_t mode);

/* Iterative bottom-up mergesort */
void merge_sort(int arr[], int n, merge_mode_t reverse) {
    

    for (int curr_size = 1; curr_size < n; curr_size *= 2) {
        for (int left_start = 0; left_start < n - 1; left_start += 2 * curr_size) {
            int mid = min_int(left_start + curr_size - 1, n - 1);
            int right_end = min_int(left_start + 2 * curr_size - 1, n - 1);

            if (mid < right_end) { // Only merge if there's something to merge
                printf("calling merge with l=%d, m=%d, r=%d, bitmap_used=%u, current_bitmap=%u\n", left_start, mid,
                       right_end, bitmap_used, sort_info.bitmap[bitmap_used]);
                merge(arr, left_start, mid, right_end, reverse);
            }
        }
        break;
    }
}

/* Merge function (records or replays bitmap decisions) */
static void merge(int arr[], int l, int m, int r, merge_mode_t mode) {
    int n1 = m - l + 1;
    int n2 = r - m;

    int *L = malloc(n1 * sizeof(int));
    int *R = malloc(n2 * sizeof(int));
    if (!L || !R) {
        perror("malloc");
        exit(1);
    }

    for (int i = 0; i < n1; i++)
        L[i] = arr[l + i];
    for (int j = 0; j < n2; j++)
        R[j] = arr[m + 1 + j];

    int i = 0, j = 0, k = l;

    while (i < n1 && j < n2) {
        if (mode == MERGE_FORWARD) {
            if (L[i] <= R[j]) {
                sort_info.bitmap[sort_info.bitmap_size++] = 1;
                arr[k++] = L[i++];
            } else {
                sort_info.bitmap[sort_info.bitmap_size++] = 0;
                arr[k++] = R[j++];
            }
        } else { /* MERGE_REVERSE */
            if (bitmap_used >= sort_info.bitmap_size) {
                fprintf(stderr, "Bitmap underflow\n");
                exit(1);
            }
            uint8_t decision = sort_info.bitmap[bitmap_used++];
            if (decision == 1) {
                arr[k++] = R[j++];
            } else {
                arr[k++] = L[i++];
            }
        }
    }

    while (i < n1)
        arr[k++] = L[i++];
    while (j < n2)
        arr[k++] = R[j++];

    free(L);
    free(R);
}

/* Print helpers */
static void print_int_array(const int *A, size_t size) {
    for (size_t i = 0; i < size; i++)
        printf("%d ", A[i]);
    printf("\n");
}
static void print_bitmap(const uint8_t *A, size_t size) {
    for (size_t i = 0; i < size; i++)
        printf("%u ", A[i]);
    printf("\n");
}

/* Driver */
int main(void) {
    int arr[] = {12, 11, 13, 5, 6, 7, 9};
    size_t n = sizeof(arr) / sizeof(arr[0]);

    sort_info.original_size = n;
    sort_info.bitmap_size = 0;

    printf("Original array:\n");
    print_int_array(arr, n);

    merge_sort(arr, n, 0);

    printf("\nSorted array:\n");
    print_int_array(arr, n);

    printf("\nBitmap decisions (%zu):\n", sort_info.bitmap_size);
    print_bitmap(sort_info.bitmap, sort_info.bitmap_size);

    merge_sort(arr, n, 1);

    printf("\nReconstructed original array:\n");
    print_int_array(arr, n);

    return 0;
}