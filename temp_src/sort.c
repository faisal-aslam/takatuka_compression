// src/iterative_rle/sort.c

#include "sort.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SortInfo sort_info;

/* Utility */
static inline int min_int(int x, int y) { return (x < y) ? x : y; }

typedef enum { MERGE_FORWARD = 0, MERGE_REVERSE = 1 } merge_mode_t;

/* Forward declarations */
static void merge(int arr[], int l, int m, int r);
static void merge_reverse(int arr[], int l, int m, int r);
static void print_int_array(const int *A, size_t size);
static void print_bitmap(const uint8_t *A, size_t size);

/* Iterative bottom-up mergesort */
void merge_sort(int arr[], int n, int max_stages) {
    int stages = -1;
    for (int curr_size = 1; curr_size < n; curr_size *= 2) {
        printf("\n\n************************* curr_size=%d************ \n\n", curr_size);
        // if (stages++ >= max_stages) break;
        for (int left_start = 0; left_start < n - 1; left_start += 2 * curr_size) {
            int mid = min_int(left_start + curr_size - 1, n - 1);
            int right_end = min_int(left_start + 2 * curr_size - 1, n - 1);

            if (mid < right_end) { // Only merge if there's something to merge
                printf("calling merge with l=%d, m=%d, r=%d, bitmap_size=%zu\n", left_start, mid, right_end,
                       sort_info.bitmap_size);
                merge(arr, left_start, mid, right_end);
            }
            printf("***** After curr_size=%u\n", curr_size);
            print_int_array(arr, n);
            printf("******* \n\n");
        }
    }
}

/* Merge function (records or replays bitmap decisions) */
static void merge(int arr[], int l, int m, int r) {
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
    if (i == n1 || j == n2) {
        sort_info.bitmap[sort_info.bitmap_size++] = 0;
    }

    while (i < n1 && j < n2) {

        if (L[i] <= R[j]) {
            sort_info.bitmap[sort_info.bitmap_size++] = 0;
            printf(" current bitmap = %u\n", sort_info.bitmap[sort_info.bitmap_size - 1]);
            arr[k++] = L[i++];
        } else {
            sort_info.bitmap[sort_info.bitmap_size++] = 1;
            printf(" current bitmap = %u\n", sort_info.bitmap[sort_info.bitmap_size - 1]);
            arr[k++] = R[j++];
        }
    }

    while (i < n1) {
        arr[k++] = L[i++];
    }
    while (j < n2) {
        arr[k++] = R[j++];
    }

    free(L);
    free(R);
}

/* Iterative bottom-up mergesort - REVERSE */
void merge_sort_reverse(int arr[], int n) {
    // Start with largest subarray size and work backwards
    int max_size = 1;
    while (max_size < n) {
        max_size *= 2;
    }
    max_size /= 2; // Largest power of 2 <= n

    for (int curr_size = max_size; curr_size >= 1; curr_size /= 2) {
        if (sort_info.bitmap_size == 0) break; // we are done.
        int last_start = ((n - 2) / (2 * curr_size)) * (2 * curr_size);
        for (int left_start = last_start; left_start >= 0; left_start -= 2 * curr_size) {
            //  for (int left_start = 0; left_start < n - 1; left_start += 2 * curr_size) {
            if (sort_info.bitmap_size == 0) break; // we are done.
            int mid = min_int(left_start + curr_size - 1, n - 1);
            int right_end = min_int(left_start + 2 * curr_size - 1, n - 1);

            if (mid < right_end) {
                printf("calling merge with l=%d, m=%d, r=%d, bitmap_size=%zu\n", left_start, mid, right_end,
                       sort_info.bitmap_size);
                merge_reverse(arr, left_start, mid, right_end);
            }
        }
        printf("***** After curr_size=%u\n", curr_size);
        print_int_array(arr, n);
        printf("******* \n\n");
    }
}

/* Merge function (records or replays bitmap decisions) */
static void merge_reverse(int arr[], int l, int m, int r) {
    int n1 = m - l + 1;
    int n2 = r - m;

    int *L = malloc(n1 * sizeof(int));
    int *R = malloc(n2 * sizeof(int));
    if (!L || !R) {
        perror("malloc");
        exit(1);
    }

    // step 1. Find the bitmap starting position.
    uint32_t total_bitmap = r - l;

    uint32_t bit_map_start = sort_info.bitmap_size - total_bitmap;
    printf("total_bitmap for this stage=%u, where we start at=%u\n", total_bitmap, bit_map_start);

    int i = 0, j = 0, k = l;

    while (i < n1 && j < n2) {

        uint8_t decision = sort_info.bitmap[bit_map_start++];
        printf(" current bitmap[%u]=%u\n", bit_map_start - 1, sort_info.bitmap[bit_map_start - 1]);
        print_bitmap(sort_info.bitmap, sort_info.bitmap_size);
        if (decision == 1) {
            R[j++] = arr[k++];
        } else {
            L[i++] = arr[k++];
        }
    }
    while (i < n1) {
        L[i++] = arr[k++];
    }
    while (j < n2) {
        R[j++] = arr[k++];
    }

    // finally copy it in the array the correctly created left and right.
    for (int i = 0; i < n1; i++)
        arr[l + i] = L[i];
    for (int j = 0; j < n2; j++)
        arr[m + 1 + j] = R[j];

    // change bitmap size so that the same bits are not used by other iterations.
    sort_info.bitmap_size = sort_info.bitmap_size - total_bitmap;

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
    int arr[] = {39, 27, 43, 3, 9, 82, 10};
    size_t n = sizeof(arr) / sizeof(arr[0]);

    sort_info.original_size = n;
    sort_info.bitmap_size = 0;

    printf("Original array:\n");
    print_int_array(arr, n);

    merge_sort(arr, n, 1);

    printf("\nSorted array:\n");
    print_int_array(arr, n);

    printf("\nBitmap decisions (%zu):\n", sort_info.bitmap_size);
    print_bitmap(sort_info.bitmap, sort_info.bitmap_size);

    printf("\n\n*************************Reverse is started ************************************** \n");

    merge_sort_reverse(arr, n);

    printf("\nReconstructed original array:\n");
    print_int_array(arr, n);

    return 0;
}