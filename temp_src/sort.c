// src/iterative_rle/sort.c

#include "sort.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

SortInfo sort_info;

/* Utility */
static inline int min_int(int x, int y) { return (x < y) ? x : y; }

typedef enum { MERGE_FORWARD = 0, MERGE_REVERSE = 1 } merge_mode_t;

/* Forward declarations */
static void merge(int arr[], int l, int m, int r);
static void merge_reverse(int arr[], int l, int m, int r);
static void print_int_array(const int *A, size_t size);
static void print_bitmap(const uint8_t *A, size_t size);
static int compare_arrays(const int *a, const int *b, size_t n);

/* Iterative bottom-up mergesort */
void merge_sort(int arr[], int n, int max_stages) {
    int stages = -1;
    for (int curr_size = 1; curr_size < n; curr_size *= 2) {
        // if (stages++ >= max_stages) break;
        for (int left_start = 0; left_start < n - 1; left_start += 2 * curr_size) {
            int mid = min_int(left_start + curr_size - 1, n - 1);
            int right_end = min_int(left_start + 2 * curr_size - 1, n - 1);

            if (mid < right_end) {
                merge(arr, left_start, mid, right_end);
            }
        }
    }
}

/* Merge function (records bitmap decisions) */
static void merge(int arr[], int l, int m, int r) {
    int n1 = m - l + 1;
    int n2 = r - m;

    int *L = malloc(n1 * sizeof(int));
    int *R = malloc(n2 * sizeof(int));
    if (!L || !R) {
        perror("malloc");
        exit(1);
    }

    for (int i = 0; i < n1; i++) L[i] = arr[l + i];
    for (int j = 0; j < n2; j++) R[j] = arr[m + 1 + j];

    int i = 0, j = 0, k = l;

    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) {
            sort_info.bitmap[sort_info.bitmap_size++] = 0;
            arr[k++] = L[i++];
        } else {
            sort_info.bitmap[sort_info.bitmap_size++] = 1;
            arr[k++] = R[j++];
        }
    }
    while (i < n1) arr[k++] = L[i++];
    while (j < n2) arr[k++] = R[j++];

    free(L);
    free(R);
}

/* Iterative bottom-up mergesort - REVERSE */
void merge_sort_reverse(int arr[], int n) {
    int max_size = 1;
    while (max_size < n) max_size *= 2;
    max_size /= 2;

    for (int curr_size = max_size; curr_size >= 1; curr_size /= 2) {
        if (sort_info.bitmap_size == 0) break;
        int last_start = ((n - 2) / (2 * curr_size)) * (2 * curr_size);
        for (int left_start = last_start; left_start >= 0; left_start -= 2 * curr_size) {
            if (sort_info.bitmap_size == 0) break;
            int mid = min_int(left_start + curr_size - 1, n - 1);
            int right_end = min_int(left_start + 2 * curr_size - 1, n - 1);

            if (mid < right_end) {
                merge_reverse(arr, left_start, mid, right_end);
            }
        }
    }
}

/* Merge function (replays bitmap decisions) */
static void merge_reverse(int arr[], int l, int m, int r) {
    int n1 = m - l + 1;
    int n2 = r - m;

    int *L = malloc(n1 * sizeof(int));
    int *R = malloc(n2 * sizeof(int));
    if (!L || !R) {
        perror("malloc");
        exit(1);
    }

    uint32_t total_bitmap = r - l;
    uint32_t bit_map_start = sort_info.bitmap_size - total_bitmap;

    int i = 0, j = 0, k = l;

    while (i < n1 && j < n2) {
        uint8_t decision = sort_info.bitmap[bit_map_start++];
        if (decision == 1) {
            R[j++] = arr[k++];
        } else {
            L[i++] = arr[k++];
        }
    }
    while (i < n1) L[i++] = arr[k++];
    while (j < n2) R[j++] = arr[k++];

    for (int i = 0; i < n1; i++) arr[l + i] = L[i];
    for (int j = 0; j < n2; j++) arr[m + 1 + j] = R[j];

    sort_info.bitmap_size -= total_bitmap;

    free(L);
    free(R);
}

/* Compare arrays */
static int compare_arrays(const int *a, const int *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            printf("Mismatch at index %zu: %d != %d\n", i, a[i], b[i]);
            return 0;
        }
    }
    return 1;
}

/* Print helpers */
static void print_int_array(const int *A, size_t size) {
    for (size_t i = 0; i < size; i++) printf("%d ", A[i]);
    printf("\n");
}
static void print_bitmap(const uint8_t *A, size_t size) {
    for (size_t i = 0; i < size; i++) printf("%u ", A[i]);
    printf("\n");
}

#ifdef ENABLE_SELFTEST
/* Toggle deterministic vs random seed */
#define USE_TIME_BASED_SEED 0

static void run_fuzz_tests(void) {
    const int NUM_TESTS = 5000;
    const int MAX_SIZE = 5000;
    const int VALUE_RANGE = 10000;

    if (USE_TIME_BASED_SEED)
        srand((unsigned)time(NULL));
    else
        srand(12345);

    for (int t = 0; t < NUM_TESTS; t++) {
        int n = (rand() % MAX_SIZE) + 2;
        int *arr = malloc(n * sizeof(int));
        int *orig = malloc(n * sizeof(int));
        if (!arr || !orig) { perror("malloc"); exit(1); }

        for (int i = 0; i < n; i++) {
            arr[i] = rand() % VALUE_RANGE;
            orig[i] = arr[i];
        }

        sort_info.original_size = n;
        sort_info.bitmap_size = 0;

        merge_sort(arr, n, n);
        merge_sort_reverse(arr, n);

        if (!compare_arrays(arr, orig, n)) {
            printf("\n!!! Fuzz test FAILED on test #%d, n=%d !!!\n", t, n);
            printf("Original:\n");
            print_int_array(orig, n);
            printf("Reconstructed:\n");
            print_int_array(arr, n);
            printf("Bitmap decisions (%zu):\n", sort_info.bitmap_size);
            print_bitmap(sort_info.bitmap, sort_info.bitmap_size);
            exit(1);
        }

        free(arr);
        free(orig);
    }
    printf("\nAll %d fuzz tests passed successfully!\n", NUM_TESTS);
}
#endif

/* Driver */
int main(void) {
#ifdef ENABLE_SELFTEST
    run_fuzz_tests();
#else
    int arr[] = {39, 27, 43, 3, 9, 82, 10, 11};
    size_t n = sizeof(arr) / sizeof(arr[0]);

    int *orig = malloc(n * sizeof(int));
    memcpy(orig, arr, n * sizeof(int));

    sort_info.original_size = n;
    sort_info.bitmap_size = 0;

    printf("Original array:\n");
    print_int_array(arr, n);

    merge_sort(arr, n, 1);

    printf("\nSorted array:\n");
    print_int_array(arr, n);

    printf("\nBitmap decisions (%zu):\n", sort_info.bitmap_size);
    print_bitmap(sort_info.bitmap, sort_info.bitmap_size);

    merge_sort_reverse(arr, n);

    printf("\nReconstructed original array:\n");
    print_int_array(arr, n);

    if (!compare_arrays(arr, orig, n)) {
        printf("\nRound-trip FAILED!\n");
    } else {
        printf("\nRound-trip verified OK.\n");
    }
    free(orig);
#endif
    return 0;
}
