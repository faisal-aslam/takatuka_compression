// src/iterative_rle/sort.c

// src/iterative_rle/sort.c

#include "sort.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

SortInfo sort_info;

/* Utility */
static inline int min_int(int x, int y) { return (x < y) ? x : y; }

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
        if (stages++ >= max_stages) break;
#ifdef DEBUG
        printf("\n=== Forward merge stage (curr_size=%d) ===\n", curr_size);
#endif
        for (int left_start = 0; left_start < n - 1; left_start += 2 * curr_size) {
            int mid = min_int(left_start + curr_size - 1, n - 1);
            int right_end = min_int(left_start + 2 * curr_size - 1, n - 1);
            if (mid < right_end) {
#ifdef DEBUG
                printf("Calling merge: l=%d m=%d r=%d\n", left_start, mid, right_end);
                printf("Array before merge: ");
                print_int_array(arr + left_start, right_end - left_start + 1);
#endif
                merge(arr, left_start, mid, right_end);
#ifdef DEBUG
                printf("Array after merge: ");
                print_int_array(arr + left_start, right_end - left_start + 1);
                printf("Global bitmap now (%zu): ", sort_info.bitmap_size);
                print_bitmap(sort_info.bitmap, sort_info.bitmap_size);
#endif
            }
        }
    }
}

/* Merge function (records bitmap decisions in *reverse* order) */
static void merge(int arr[], int l, int m, int r) {
    int n1 = m - l + 1;
    int n2 = r - m;

    int *L = malloc(n1 * sizeof(int));
    int *R = malloc(n2 * sizeof(int));
    if (!L || !R) { perror("malloc"); exit(1); }

    for (int i = 0; i < n1; i++) L[i] = arr[l + i];
    for (int j = 0; j < n2; j++) R[j] = arr[m + 1 + j];

    int i = 0, j = 0, k = l;

    uint8_t *tmp_bits = malloc((n1 + n2) * sizeof(uint8_t));
    int bit_count = 0;

    while (i < n1 && j < n2) {
#ifdef DEBUG
        printf("Compare L[%d]=%d vs R[%d]=%d\n", i, L[i], j, R[j]);
#endif
        if (L[i] <= R[j]) {
            tmp_bits[bit_count++] = 0;
            arr[k++] = L[i++];
#ifdef DEBUG
            printf(" -> chose LEFT, wrote 0\n");
#endif
        } else {
            tmp_bits[bit_count++] = 1;
            arr[k++] = R[j++];
#ifdef DEBUG
            printf(" -> chose RIGHT, wrote 1\n");
#endif
        }
    }
    while (i < n1) arr[k++] = L[i++];
    while (j < n2) arr[k++] = R[j++];

#ifdef DEBUG
    printf("Temporary decision bits (forward order): ");
    for (int b = 0; b < bit_count; b++) printf("%u ", tmp_bits[b]);
    printf("\n");
#endif

    for (int b = bit_count - 1; b >= 0; b--) {
        sort_info.bitmap[sort_info.bitmap_size++] = tmp_bits[b];
    }

#ifdef DEBUG
    printf("Flushed bits in reverse. Bitmap now (%zu): ", sort_info.bitmap_size);
    print_bitmap(sort_info.bitmap, sort_info.bitmap_size);
#endif

    free(tmp_bits);
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
#ifdef DEBUG
        printf("\n=== Reverse merge stage (curr_size=%d) ===\n", curr_size);
#endif
        int last_start = ((n - 2) / (2 * curr_size)) * (2 * curr_size);
        for (int left_start = last_start; left_start >= 0; left_start -= 2 * curr_size) {
            if (sort_info.bitmap_size == 0) break;
            int mid = min_int(left_start + curr_size - 1, n - 1);
            int right_end = min_int(left_start + 2 * curr_size - 1, n - 1);
            if (mid < right_end) {
#ifdef DEBUG
                printf("Calling merge_reverse: l=%d m=%d r=%d\n", left_start, mid, right_end);
                printf("Array before reverse merge: ");
                print_int_array(arr + left_start, right_end - left_start + 1);
                printf("Bitmap size=%zu\n", sort_info.bitmap_size);
#endif
                merge_reverse(arr, left_start, mid, right_end);
#ifdef DEBUG
                printf("Array after reverse merge: ");
                print_int_array(arr + left_start, right_end - left_start + 1);
                printf("Bitmap size=%zu\n", sort_info.bitmap_size);
#endif
            }
        }
    }
}

/* Reverse merge (replays by consuming bits backward) */
static void merge_reverse(int arr[], int l, int m, int r) {
    int n1 = m - l + 1;
    int n2 = r - m;

    int *L = malloc(n1 * sizeof(int));
    int *R = malloc(n2 * sizeof(int));
    if (!L || !R) { perror("malloc"); exit(1); }

    int i = 0, j = 0, k = l;

    while (i < n1 && j < n2) {
        uint8_t decision = sort_info.bitmap[--sort_info.bitmap_size];
#ifdef DEBUG
        printf("Consume bitmap[%zu]=%u\n", sort_info.bitmap_size, decision);
#endif
        if (decision == 0) {
            L[i++] = arr[k++];
#ifdef DEBUG
            printf(" -> placed in LEFT[%d]\n", i - 1);
#endif
        } else {
            R[j++] = arr[k++];
#ifdef DEBUG
            printf(" -> placed in RIGHT[%d]\n", j - 1);
#endif
        }
    }
    while (i < n1) L[i++] = arr[k++];
    while (j < n2) R[j++] = arr[k++];

    for (int ii = 0; ii < n1; ii++) arr[l + ii] = L[ii];
    for (int jj = 0; jj < n2; jj++) arr[m + 1 + jj] = R[jj];

    free(L);
    free(R);
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

/* Compare helpers */
static int compare_arrays(const int *a, const int *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

/* Randomized fuzz testing harness */
static void fuzz_test(int max_n, int iterations) {
    srand(12345);
    int *orig = malloc(max_n * sizeof(int));
    int *arr = malloc(max_n * sizeof(int));
    int *restored = malloc(max_n * sizeof(int));

    for (int t = 0; t < iterations; t++) {
        int n = rand() % max_n + 1;
        for (int i = 0; i < n; i++) orig[i] = rand() % 1000;

        memcpy(arr, orig, n * sizeof(int));
        sort_info.bitmap_size = 0;
        sort_info.original_size = n;

        merge_sort(arr, n, n);

        memcpy(restored, arr, n * sizeof(int));
        merge_sort_reverse(restored, n);

        if (!compare_arrays(orig, restored, n)) {
            printf("Fuzz test failed! Iteration=%d n=%d\n", t, n);
            printf("Original: "); print_int_array(orig, n);
            printf("Restored: "); print_int_array(restored, n);
            exit(1);
        }

        if (t % 500 == 0) {
            printf("Fuzz iteration %d passed (n=%d)\n", t, n);
        }
    }

    free(orig);
    free(arr);
    free(restored);
}

/* Driver */
int main(void) {
    int arr[] = {39, 27, 43, 3, 9, 82, 10};
    size_t n = sizeof(arr) / sizeof(arr[0]);

    sort_info.original_size = n;
    sort_info.bitmap_size = 0;

    printf("Original array:\n");
    print_int_array(arr, n);

    merge_sort(arr, n, n);

    printf("\nSorted array:\n");
    print_int_array(arr, n);

    printf("\nBitmap decisions (%zu):\n", sort_info.bitmap_size);
    print_bitmap(sort_info.bitmap, sort_info.bitmap_size);

    printf("\n\n************************* Reverse is started ************************************** \n");

    merge_sort_reverse(arr, n);

    printf("\nReconstructed original array:\n");
    print_int_array(arr, n);

    //printf("\nStarting fuzz tests...\n");
    //fuzz_test(2000, 2000);
    //printf("All fuzz tests passed!\n");

    return 0;
}
