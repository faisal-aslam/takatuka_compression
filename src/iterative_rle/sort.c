// src/iterative_rle/sort.c

#include "sort.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DEBUG_BITMAP 1

#if DEBUG_BITMAP
#define DBG_PRINT(...) printf(__VA_ARGS__)
#else
#define DBG_PRINT(...)
#endif

/* ---- Internal helpers ---- */
static inline void bit_set(uint8_t *bitmap, uint32_t bitpos, int bit) {
    uint32_t byte = bitpos >> 3;
    uint32_t shift = 7 - (bitpos & 7);
    if (bit) {
        bitmap[byte] |= (1u << shift);
    } else {
        bitmap[byte] &= ~(1u << shift);
    }
}

static inline int bit_get(const uint8_t *bitmap, uint32_t bitpos) {
    uint32_t byte = bitpos >> 3;
    uint32_t shift = 7 - (bitpos & 7);
    return (bitmap[byte] >> shift) & 1;
}

static inline void bw_write(uint8_t *bitmap, uint32_t *bitpos, int bit) {
    bit_set(bitmap, *bitpos, bit);
    (*bitpos)++;
}

/* ---- Recursive merge sort with deterministic order ---- */
static void recursive_merge_sort(uint8_t *arr, uint8_t *temp, uint32_t left, uint32_t right, uint8_t *bitmap,
                                 uint32_t *bitpos, uint32_t max_comparisons, uint32_t *comparisons_so_far) {
    if (left >= right) return;
    if (max_comparisons > 0 && *comparisons_so_far >= max_comparisons) return;

    uint32_t mid = left + (right - left) / 2;

    // First recursively sort both halves
    recursive_merge_sort(arr, temp, left, mid, bitmap, bitpos, max_comparisons, comparisons_so_far);
    if (max_comparisons > 0 && *comparisons_so_far >= max_comparisons) return;

    recursive_merge_sort(arr, temp, mid + 1, right, bitmap, bitpos, max_comparisons, comparisons_so_far);
    if (max_comparisons > 0 && *comparisons_so_far >= max_comparisons) return;

    // Then merge them
    uint32_t i = left, j = mid + 1, k = left;

    while (i <= mid && j <= right) {
        if (max_comparisons > 0 && *comparisons_so_far >= max_comparisons) break;

        if (arr[i] <= arr[j]) {
            DBG_PRINT("[WRITE] bitpos=%u write=0 (LEFT)  left_idx=%u left_val=%u right_idx=%u right_val=%u\n", *bitpos,
                      i, (unsigned)arr[i], j, (unsigned)arr[j]);
            temp[k++] = arr[i++];
            bw_write(bitmap, bitpos, 0);
        } else {
            DBG_PRINT("[WRITE] bitpos=%u write=1 (RIGHT) left_idx=%u left_val=%u right_idx=%u right_val=%u\n", *bitpos,
                      i, (unsigned)arr[i], j, (unsigned)arr[j]);
            temp[k++] = arr[j++];
            bw_write(bitmap, bitpos, 1);
        }
        (*comparisons_so_far)++;
    }

    // Copy remaining elements
    while (i <= mid)
        temp[k++] = arr[i++];
    while (j <= right)
        temp[k++] = arr[j++];

    // Copy back to original array
    for (uint32_t idx = left; idx <= right; idx++) {
        arr[idx] = temp[idx];
    }
}

/* ---- Reverse merge using iterative approach ---- */
static void reverse_merge_iterative(uint8_t *arr, uint8_t *temp, uint32_t size, const uint8_t *bitmap, uint32_t *bitpos) {
    // First, copy the sorted array to temp
    memcpy(temp, arr, size * sizeof(uint8_t));
    
    // We'll reconstruct by working backwards through the merge process
    // Start with the smallest merge size and work up
    for (uint32_t merge_size = 1; merge_size < size; merge_size *= 2) {
        for (uint32_t left = 0; left < size; left += 2 * merge_size) {
            uint32_t mid = left + merge_size;
            uint32_t right = left + 2 * merge_size - 1;
            if (right >= size) right = size - 1;
            
            if (mid > right) continue;
            
            uint32_t i = left, j = mid, k = left;
            
            while (i < mid && j <= right) {
                if (*bitpos == 0) {
                    // No more bits, just copy remaining elements
                    break;
                }
                
                (*bitpos)--;
                int bit = bit_get(bitmap, *bitpos);
                
                DBG_PRINT("[READ ] pop bitpos=%u read=%d\n", *bitpos, bit);
                
                if (bit == 0) {
                    // Element came from left half
                    arr[k++] = temp[i++];
                } else {
                    // Element came from right half
                    arr[k++] = temp[j++];
                }
            }
            
            // Copy remaining elements
            while (i < mid) arr[k++] = temp[i++];
            while (j <= right) arr[k++] = temp[j++];
        }
        
        // Copy back to temp for next iteration
        memcpy(temp, arr, size * sizeof(uint8_t));
    }
}

void reconstruct_original(uint8_t *sorted_data, const SortHeader *header) {
    if (!header || header->original_size <= 1) return;

    uint32_t size = header->original_size;
    uint8_t *temp = malloc(size * sizeof(uint8_t));
    if (!temp) return;

    /* Start popping from the end (the total number of bits written). */
    uint32_t bitpos = header->bitmap_bits;
    reverse_merge_iterative(sorted_data, temp, size, header->bitmap, &bitpos);

    free(temp);
}

/* ---- Public API ---- */
SortHeader *partial_merge_sort(uint8_t *data, uint32_t size, uint32_t max_comparisons) {
    if (size <= 1) return NULL;

    SortHeader *header = malloc(sizeof(SortHeader));
    if (!header) return NULL;

    header->original_size = size;
    header->bitmap_bits = 0;

    /* Conservative maximum bits required: size * 32 is overkill but safe. */
    uint32_t max_possible_bits = size * 32u;
    uint32_t bitmap_bytes = (max_possible_bits + 7) / 8;
    header->bitmap = calloc(bitmap_bytes, 1);
    if (!header->bitmap) {
        free(header);
        return NULL;
    }

    uint8_t *temp = malloc(size * sizeof(uint8_t));
    if (!temp) {
        free(header->bitmap);
        free(header);
        return NULL;
    }

    uint32_t bitpos = 0;
    uint32_t comparisons = 0;

    recursive_merge_sort(data, temp, 0, size - 1, header->bitmap, &bitpos, max_comparisons, &comparisons);

    header->bitmap_bits = bitpos;
    free(temp);
    return header;
}

void free_sort_header(SortHeader *header) {
    if (header) {
        if (header->bitmap) free(header->bitmap);
        free(header);
    }
}

uint32_t bitmap_bytes_needed(uint32_t bit_count) { return (bit_count + 7) / 8; }

/***************** sorting_main.c */

// sorting_main.c - test harness
#include <time.h>

/* ---- Test Utilities ---- */

void print_array(const char *label, const uint8_t *data, uint32_t size) {
    printf("%s: ", label);
    for (uint32_t i = 0; i < size; i++) {
        printf("%u ", data[i]);
    }
    printf("\n");
}

int is_sorted(const uint8_t *data, uint32_t size) {
    for (uint32_t i = 1; i < size; i++) {
        if (data[i] < data[i - 1]) {
            return 0;
        }
    }
    return 1;
}

int arrays_equal(const uint8_t *a, const uint8_t *b, uint32_t size) { return memcmp(a, b, size) == 0; }

void generate_random_data(uint8_t *data, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        data[i] = rand() % 256;
    }
}

void generate_sorted_data(uint8_t *data, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        data[i] = i % 256;
    }
}

void generate_reverse_sorted_data(uint8_t *data, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        data[i] = (255 - i) % 256;
    }
}

/* ---- Test Cases ---- */

void test_full_sort() {
    printf("=== Test 1: Full Sort ===\n");

    uint8_t original[] = {5, 2, 9, 1, 6, 3};
    uint32_t size = sizeof(original) / sizeof(original[0]);

    uint8_t *data = malloc(size);
    memcpy(data, original, size);
    print_array("Original", data, size);

    SortHeader *header = partial_merge_sort(data, size, 0); // full sort
    print_array("Sorted", data, size);

    printf("Is sorted: %s\n", is_sorted(data, size) ? "YES" : "NO");
    if (header) {
        printf("Bitmap bits: %u\n", header->bitmap_bits);
    } else {
        printf("No header returned\n");
    }

    // Test reconstruction
    if (header) {
        reconstruct_original(data, header);
        print_array("Reconstructed", data, size);

        printf("Match original: %s\n", arrays_equal(data, original, size) ? "YES" : "NO");
    }

    free(data);
    free_sort_header(header);
    printf("\n");
}

void test_partial_sort() {
    printf("=== Test 2: Partial Sort ===\n");

    uint8_t original[] = {5, 2, 9, 1, 6, 3, 13, 4, 5, 0, 1, 19, 20};
    uint32_t size = sizeof(original) / sizeof(original[0]);
    uint8_t *data = malloc(size);

    memcpy(data, original, size);
    print_array("Original", data, size);

    uint32_t max_comparisons = 5;
    SortHeader *header = partial_merge_sort(data, size, max_comparisons);
    print_array("Partially Sorted", data, size);

    if (header) {
        printf("Comparisons made (bits): %u (limit: %u)\n", header->bitmap_bits, max_comparisons);
    } else {
        printf("No header returned\n");
    }
    printf("Is sorted: %s\n", is_sorted(data, size) ? "YES" : "NO");

    // Test reconstruction
    if (header) {
        reconstruct_original(data, header);
        print_array("Reconstructed", data, size);

        printf("Match original: %s\n", arrays_equal(data, original, size) ? "YES" : "NO");
    }

    free(data);
    free_sort_header(header);
    printf("\n");
}

void test_single_element() {
    printf("=== Test 3: Single Element ===\n");

    uint8_t original[] = {42};
    uint32_t size = 1;
    uint8_t *data = malloc(size);
    memcpy(data, original, size);
    print_array("Original", data, size);

    SortHeader *header = partial_merge_sort(data, size, 0);
    print_array("Sorted", data, size);

    if (header) {
        printf("Bitmap bits: %u\n", header->bitmap_bits);
        reconstruct_original(data, header);
        print_array("Reconstructed", data, size);
        printf("Match original: %s\n", arrays_equal(data, original, size) ? "YES" : "NO");
        free_sort_header(header);
    } else {
        printf("No header (expected for single element)\n");
    }

    free(data);
    printf("\n");
}

void test_empty_array() {
    printf("=== Test 4: Empty Array ===\n");

    uint32_t size = 0;
    uint8_t *data = NULL;

    SortHeader *header = partial_merge_sort(data, size, 0);

    if (header) {
        printf("Unexpected header for empty array\n");
        free_sort_header(header);
    } else {
        printf("No header (expected for empty array)\n");
    }

    printf("\n");
}

void test_random_data() {
    printf("=== Test 5: Random Data ===\n");

    uint32_t size = 20;
    uint8_t *original = malloc(size);
    uint8_t *data = malloc(size);

    generate_random_data(original, size);
    memcpy(data, original, size);
    print_array("Original", data, size);

    SortHeader *header = partial_merge_sort(data, size, 0); // full sort
    print_array("Sorted", data, size);

    printf("Is sorted: %s\n", is_sorted(data, size) ? "YES" : "NO");
    if (header) printf("Bitmap bits: %u\n", header->bitmap_bits);

    // Test reconstruction
    if (header) {
        reconstruct_original(data, header);
        print_array("Reconstructed", data, size);
        printf("Match original: %s\n", arrays_equal(data, original, size) ? "YES" : "NO");
    }

    free(original);
    free(data);
    free_sort_header(header);
    printf("\n");
}

void test_already_sorted() {
    printf("=== Test 6: Already Sorted Data ===\n");

    uint32_t size = 15;
    uint8_t *original = malloc(size);
    uint8_t *data = malloc(size);

    generate_sorted_data(original, size);
    memcpy(data, original, size);
    print_array("Original", data, size);

    SortHeader *header = partial_merge_sort(data, size, 0);
    print_array("Sorted", data, size);

    printf("Is sorted: %s\n", is_sorted(data, size) ? "YES" : "NO");
    if (header) printf("Bitmap bits: %u\n", header->bitmap_bits);

    if (header) {
        reconstruct_original(data, header);
        print_array("Reconstructed", data, size);
        printf("Match original: %s\n", arrays_equal(data, original, size) ? "YES" : "NO");
    }

    free(original);
    free(data);
    free_sort_header(header);
    printf("\n");
}

void test_reverse_sorted() {
    printf("=== Test 7: Reverse Sorted Data ===\n");

    uint32_t size = 15;
    uint8_t *original = malloc(size);
    uint8_t *data = malloc(size);

    generate_reverse_sorted_data(original, size);
    memcpy(data, original, size);
    print_array("Original", data, size);

    SortHeader *header = partial_merge_sort(data, size, 0);
    print_array("Sorted", data, size);

    printf("Is sorted: %s\n", is_sorted(data, size) ? "YES" : "NO");
    if (header) printf("Bitmap bits: %u\n", header->bitmap_bits);

    if (header) {
        reconstruct_original(data, header);
        print_array("Reconstructed", data, size);
        printf("Match original: %s\n", arrays_equal(data, original, size) ? "YES" : "NO");
    }

    free(original);
    free(data);
    free_sort_header(header);
    printf("\n");
}

void test_various_partial_limits() {
    printf("=== Test 8: Various Partial Limits ===\n");

    uint8_t original[] = {5, 2, 9, 1, 6, 3, 13, 4, 5, 0, 1, 19, 20};
    uint32_t size = sizeof(original) / sizeof(original[0]);

    uint32_t limits[] = {1, 3, 5, 10, 20, 0}; // 0 = no limit
    int num_limits = sizeof(limits) / sizeof(limits[0]);

    for (int i = 0; i < num_limits; i++) {
        uint8_t *data = malloc(size);
        memcpy(data, original, size);

        printf("Limit: %u comparisons\n", limits[i]);
        SortHeader *header = partial_merge_sort(data, size, limits[i]);

        if (header) {
            printf("Comparisons made: %u\n", header->bitmap_bits);
            print_array("Result", data, size);

            reconstruct_original(data, header);
            printf("Reconstruction %s\n", arrays_equal(data, original, size) ? "SUCCESS" : "FAILED");

            free_sort_header(header);
        } else {
            printf("No header returned\n");
        }

        free(data);
        printf("---\n");
    }
    printf("\n");
}

void test_bitmap_integrity() {
    printf("=== Test 9: Bitmap Integrity ===\n");

    uint8_t original[] = {5, 2, 9, 1, 6};
    uint32_t size = sizeof(original) / sizeof(original[0]);
    uint8_t *data = malloc(size);

    memcpy(data, original, size);
    print_array("Original", data, size);

    SortHeader *header = partial_merge_sort(data, size, 0);
    print_array("Sorted", data, size);

    if (header) {
        printf("Bitmap bits: %u\n", header->bitmap_bits);
        printf("Bitmap bytes: %u\n", bitmap_bytes_needed(header->bitmap_bits));

        printf("Bitmap content: ");
        for (uint32_t i = 0; i < bitmap_bytes_needed(header->bitmap_bits); i++) {
            printf("%02X ", header->bitmap[i]);
        }
        printf("\n");

        for (int i = 0; i < 3; i++) {
            reconstruct_original(data, header);
            printf("Reconstruction %d: %s\n", i + 1, arrays_equal(data, original, size) ? "SUCCESS" : "FAILED");
        }
    } else {
        printf("No header returned\n");
    }

    free(data);
    free_sort_header(header);
    printf("\n");
}

/* ---- Main Test Runner ---- */
int main() {
    printf("Merge Sort with Bitmap - Unit Tests\n");
    printf("===================================\n\n");

    srand((unsigned)time(NULL));

    test_full_sort();
    abort();
    test_partial_sort();
    test_single_element();
    test_empty_array();
    test_random_data();
    test_already_sorted();
    test_reverse_sorted();
    test_various_partial_limits();
    test_bitmap_integrity();

    printf("All tests completed!\n");
    return 0;
}
