#include "sort.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---- Test Utilities ---- */

// Print array
void print_array(const char *label, const uint8_t *data, uint32_t size) {
    printf("%s: ", label);
    for (uint32_t i = 0; i < size; i++) {
        printf("%u ", data[i]);
    }
    printf("\n");
}

// Check if array is sorted
int is_sorted(const uint8_t *data, uint32_t size) {
    for (uint32_t i = 1; i < size; i++) {
        if (data[i] < data[i-1]) {
            return 0;
        }
    }
    return 1;
}

// Check if two arrays are equal
int arrays_equal(const uint8_t *a, const uint8_t *b, uint32_t size) {
    return memcmp(a, b, size) == 0;
}

// Generate random test data
void generate_random_data(uint8_t *data, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        data[i] = rand() % 256;
    }
}

// Generate sorted test data
void generate_sorted_data(uint8_t *data, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        data[i] = i % 256;
    }
}

// Generate reverse sorted test data
void generate_reverse_sorted_data(uint8_t *data, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        data[i] = (255 - i) % 256;
    }
}

/* ---- Test Cases ---- */

void test_full_sort() {
    printf("=== Test 1: Full Sort ===\n");
    
    uint8_t original[] = {5, 2, 9, 1, 6, 3, 13, 4, 5, 0, 1, 19, 20};
    uint32_t size = sizeof(original);
    uint8_t *data = malloc(size);
    
    memcpy(data, original, size);
    print_array("Original", data, size);
    
    SortHeader *header = partial_merge_sort(data, size, 0); // 0 = full sort
    print_array("Sorted", data, size);
    
    printf("Is sorted: %s\n", is_sorted(data, size) ? "YES" : "NO");
    printf("Bitmap bits: %u\n", header->bitmap_bits);
    
    // Test reconstruction
    reconstruct_original(data, header);
    print_array("Reconstructed", data, size);
    
    printf("Match original: %s\n", arrays_equal(data, original, size) ? "YES" : "NO");
    
    free(data);
    free_sort_header(header);
    printf("\n");
}

void test_partial_sort() {
    printf("=== Test 2: Partial Sort ===\n");
    
    uint8_t original[] = {5, 2, 9, 1, 6, 3, 13, 4, 5, 0, 1, 19, 20};
    uint32_t size = sizeof(original);
    uint8_t *data = malloc(size);
    
    memcpy(data, original, size);
    print_array("Original", data, size);
    
    uint32_t max_comparisons = 5;
    SortHeader *header = partial_merge_sort(data, size, max_comparisons);
    print_array("Partially Sorted", data, size);
    
    printf("Comparisons made: %u (limit: %u)\n", header->bitmap_bits, max_comparisons);
    printf("Is sorted: %s\n", is_sorted(data, size) ? "YES" : "NO");
    
    // Test reconstruction
    reconstruct_original(data, header);
    print_array("Reconstructed", data, size);
    
    printf("Match original: %s\n", arrays_equal(data, original, size) ? "YES" : "NO");
    
    free(data);
    free_sort_header(header);
    printf("\n");
}

void test_single_element() {
    printf("=== Test 3: Single Element ===\n");
    
    uint8_t original[] = {42};
    uint32_t size = sizeof(original);
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
    
    uint8_t original[] = {};
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
    
    SortHeader *header = partial_merge_sort(data, size, 0); // Full sort
    print_array("Sorted", data, size);
    
    printf("Is sorted: %s\n", is_sorted(data, size) ? "YES" : "NO");
    printf("Bitmap bits: %u\n", header->bitmap_bits);
    
    // Test reconstruction
    reconstruct_original(data, header);
    print_array("Reconstructed", data, size);
    
    printf("Match original: %s\n", arrays_equal(data, original, size) ? "YES" : "NO");
    
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
    
    SortHeader *header = partial_merge_sort(data, size, 0); // Full sort
    print_array("Sorted", data, size);
    
    printf("Is sorted: %s\n", is_sorted(data, size) ? "YES" : "NO");
    printf("Bitmap bits: %u\n", header->bitmap_bits);
    
    // Test reconstruction
    reconstruct_original(data, header);
    print_array("Reconstructed", data, size);
    
    printf("Match original: %s\n", arrays_equal(data, original, size) ? "YES" : "NO");
    
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
    
    SortHeader *header = partial_merge_sort(data, size, 0); // Full sort
    print_array("Sorted", data, size);
    
    printf("Is sorted: %s\n", is_sorted(data, size) ? "YES" : "NO");
    printf("Bitmap bits: %u\n", header->bitmap_bits);
    
    // Test reconstruction
    reconstruct_original(data, header);
    print_array("Reconstructed", data, size);
    
    printf("Match original: %s\n", arrays_equal(data, original, size) ? "YES" : "NO");
    
    free(original);
    free(data);
    free_sort_header(header);
    printf("\n");
}

void test_various_partial_limits() {
    printf("=== Test 8: Various Partial Limits ===\n");
    
    uint8_t original[] = {5, 2, 9, 1, 6, 3, 13, 4, 5, 0, 1, 19, 20};
    uint32_t size = sizeof(original);
    
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
            
            // Test reconstruction
            reconstruct_original(data, header);
            printf("Reconstruction %s\n", arrays_equal(data, original, size) ? "SUCCESS" : "FAILED");
            
            free_sort_header(header);
        }
        
        free(data);
        printf("---\n");
    }
    printf("\n");
}

void test_bitmap_integrity() {
    printf("=== Test 9: Bitmap Integrity ===\n");
    
    uint8_t original[] = {5, 2, 9, 1, 6};
    uint32_t size = sizeof(original);
    uint8_t *data = malloc(size);
    
    memcpy(data, original, size);
    print_array("Original", data, size);
    
    SortHeader *header = partial_merge_sort(data, size, 0); // Full sort
    print_array("Sorted", data, size);
    
    printf("Bitmap bits: %u\n", header->bitmap_bits);
    printf("Bitmap bytes: %u\n", bitmap_bytes_needed(header->bitmap_bits));
    
    // Print bitmap content
    printf("Bitmap content: ");
    for (uint32_t i = 0; i < bitmap_bytes_needed(header->bitmap_bits); i++) {
        printf("%02X ", header->bitmap[i]);
    }
    printf("\n");
    
    // Test multiple reconstructions (should be idempotent)
    for (int i = 0; i < 3; i++) {
        reconstruct_original(data, header);
        printf("Reconstruction %d: %s\n", i+1, 
               arrays_equal(data, original, size) ? "SUCCESS" : "FAILED");
    }
    
    free(data);
    free_sort_header(header);
    printf("\n");
}

/* ---- Main Test Runner ---- */
int main() {
    printf("Merge Sort with Bitmap - Unit Tests\n");
    printf("===================================\n\n");
    
    // Seed random number generator
    srand(time(NULL));
    
    // Run all tests
    test_full_sort();
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