//iterative_rle/sort.c

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Merge operation metadata ---- */
typedef struct {
    uint32_t left;
    uint32_t mid;
    uint32_t right;
    uint32_t bit_start;
    uint32_t bit_len;
} MergeOp;

/* ---- Bitmap writer/reader helpers ---- */
static inline void bit_set(uint8_t *bitmap, uint32_t bitpos, int bit) {
    uint32_t byte = bitpos >> 3;
    uint32_t shift = 7 - (bitpos & 7);
    if (bit) bitmap[byte] |= (1u << shift);
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

/* Print bitmap in a readable format */
void print_bitmap(const uint8_t *bitmap, uint32_t bit_count) {
    printf("Bitmap (%u bits): ", bit_count);
    for (uint32_t i = 0; i < (bit_count + 7) / 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (i * 8 + j < bit_count) {
                printf("%d", (bitmap[i] >> (7 - j)) & 1);
            }
        }
        if (i < (bit_count + 7) / 8 - 1) printf(" ");
    }
    printf("\n");
}

/* ---- Forward merge that logs bits and records op metadata ---- */
static void merge_with_log(uint8_t *arr, uint8_t *temp, uint32_t left, uint32_t mid, uint32_t right, 
                          uint8_t *bitmap, uint32_t *bitpos, MergeOp *ops, uint32_t *ops_count, 
                          uint32_t max_ops, uint32_t *comparisons, uint32_t max_comparisons) {
    uint32_t start_bit = *bitpos;
    uint32_t i = left;
    uint32_t j = mid + 1;
    uint32_t k = left;

    while (i <= mid && j <= right && (*comparisons < max_comparisons || max_comparisons == 0)) {
        if (arr[i] <= arr[j]) {
            temp[k++] = arr[i++];
            bw_write(bitmap, bitpos, 0);
        } else {
            temp[k++] = arr[j++];
            bw_write(bitmap, bitpos, 1);
        }
        (*comparisons)++;
    }
    
    // Copy remaining elements if we haven't reached comparison limit
    if (*comparisons < max_comparisons || max_comparisons == 0) {
        while (i <= mid) temp[k++] = arr[i++];
        while (j <= right) temp[k++] = arr[j++];
    } else {
        // Partial merge - copy remaining elements as-is
        while (i <= mid && k <= right) temp[k++] = arr[i++];
        while (j <= right && k <= right) temp[k++] = arr[j++];
    }

    memcpy(arr + left, temp + left, (right - left + 1));

    /* record the merge operation */
    if (*ops_count < max_ops) {
        ops[*ops_count].left = left;
        ops[*ops_count].mid = mid;
        ops[*ops_count].right = right;
        ops[*ops_count].bit_start = start_bit;
        ops[*ops_count].bit_len = (*bitpos) - start_bit;
        (*ops_count)++;
    } else {
        fprintf(stderr, "merge op array overflow\n");
        exit(1);
    }
}

/* Forward iterative bottom-up merge sort with logging */
uint32_t merge_sort_with_bitmap(uint8_t *arr, uint32_t n, uint8_t *bitmap, uint32_t bitmap_bytes, 
                               MergeOp *ops, uint32_t *ops_count, uint32_t max_ops, 
                               uint32_t max_comparisons) {
    if (n <= 1) {
        *ops_count = 0;
        return 0;
    }
    
    uint8_t *temp = malloc(n);
    if (!temp) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }

    uint32_t bitpos = 0;
    uint32_t comparisons = 0;
    *ops_count = 0;

    for (uint32_t size = 1; size < n && (comparisons < max_comparisons || max_comparisons == 0); size <<= 1) {
        for (uint32_t left = 0; left + size < n && (comparisons < max_comparisons || max_comparisons == 0); left += 2 * size) {
            uint32_t mid = left + size - 1;
            uint32_t right = (left + 2 * size - 1 < n - 1) ? left + 2 * size - 1 : n - 1;
            
            if (mid < right) {
                uint32_t remaining_bits_capacity = bitmap_bytes * 8 - bitpos;
                if (remaining_bits_capacity == 0) {
                    fprintf(stderr, "bitmap too small\n");
                    free(temp);
                    exit(1);
                }
                
                merge_with_log(arr, temp, left, mid, right, bitmap, &bitpos, ops, 
                              ops_count, max_ops, &comparisons, max_comparisons);
                
                if (max_comparisons > 0 && comparisons >= max_comparisons) {
                    // Early exit due to comparison limit
                    free(temp);
                    return bitpos;
                }
            }
        }
    }

    free(temp);
    return bitpos;
}

/* ---- Reverse a single merge using the metadata ---- */
static void reverse_merge_using_op(uint8_t *arr, uint8_t *temp, const MergeOp *op, const uint8_t *bitmap) {
    uint32_t left = op->left;
    uint32_t mid = op->mid;
    uint32_t right = op->right;
    uint32_t start_bit = op->bit_start;
    uint32_t bit_len = op->bit_len;

    // Copy the merged (sorted) subarray to temp
    memcpy(temp + left, arr + left, (right - left + 1));

    uint32_t i = left;      // Pointer for left subarray
    uint32_t j = mid + 1;   // Pointer for right subarray
    
    // Reconstruct the original subarrays using the bitmap
    for (uint32_t k = 0; k < bit_len; k++) {
        int bit = bit_get(bitmap, start_bit + k);
        if (bit == 0) {
            // Left element was chosen originally
            arr[i++] = temp[left + k];
        } else {
            // Right element was chosen originally
            arr[j++] = temp[left + k];
        }
    }
    
    // Copy any remaining elements (if the merge was partial)
    while (i <= mid) {
        arr[i] = temp[left + bit_len + (i - left) - (j - (mid + 1))];
        i++;
    }
    while (j <= right) {
        arr[j] = temp[left + bit_len + (i - left) - (j - (mid + 1))];
        j++;
    }
}

/* Reverse the entire merge sequence */
void reverse_merge_sort_from_log(uint8_t *arr, uint32_t n, const MergeOp *ops, uint32_t ops_count,
                                 const uint8_t *bitmap) {
    if (n <= 1) return;
    
    uint8_t *temp = malloc(n);
    if (!temp) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }

    // Process operations in reverse order
    for (int idx = (int)ops_count - 1; idx >= 0; --idx) {
        reverse_merge_using_op(arr, temp, &ops[idx], bitmap);
    }
    
    free(temp);
}

/* ---- Demo and tests ---- */
int main(void) {
    uint8_t original[] = {5, 2, 9, 1, 6, 3, 13, 4, 5, 0, 1, 19, 20};
    uint32_t n = sizeof(original) / sizeof(original[0]);

    printf("Original array:\n");
    for (uint32_t i = 0; i < n; ++i)
        printf("%u ", original[i]);
    printf("\n\n");

    /* Test 1: full merge sort with logging */
    uint32_t bitmap_bytes = ((n * 32) + 7) / 8;
    uint8_t *bitmap = calloc(bitmap_bytes, 1);
    MergeOp *ops = malloc(sizeof(MergeOp) * (n * 2 + 10));
    uint32_t ops_count = 0;
    
    uint8_t *sorted = malloc(n);
    memcpy(sorted, original, n);
    
    uint32_t used_bits = merge_sort_with_bitmap(sorted, n, bitmap, bitmap_bytes, ops, &ops_count, 
                                               n * 2 + 10, 0); // 0 means no limit
    
    printf("Sorted array:\n");
    for (uint32_t i = 0; i < n; ++i)
        printf("%u ", sorted[i]);
    printf("\nUsed bits: %u, ops_count: %u\n", used_bits, ops_count);
    print_bitmap(bitmap, used_bits);

    // Reverse
    reverse_merge_sort_from_log(sorted, n, ops, ops_count, bitmap);
    printf("Reconstructed original:\n");
    for (uint32_t i = 0; i < n; ++i)
        printf("%u ", sorted[i]);
    printf("\nMatch original? %s\n\n", (memcmp(original, sorted, n) == 0) ? "YES" : "NO");

    /* Test 2: partial merge sort demonstration */
    uint32_t max_comparisons = 5;
    memcpy(sorted, original, n);
    memset(bitmap, 0, bitmap_bytes);
    ops_count = 0;
    
    uint32_t used_bits_partial = merge_sort_with_bitmap(sorted, n, bitmap, bitmap_bytes, ops, 
                                                       &ops_count, n * 2 + 10, max_comparisons);
    
    printf("Partially sorted with max %u comparisons:\n", max_comparisons);
    for (uint32_t i = 0; i < n; ++i)
        printf("%u ", sorted[i]);
    printf("\nUsed bits: %u, ops_count: %u\n", used_bits_partial, ops_count);
    print_bitmap(bitmap, used_bits_partial);

    // Reverse
    reverse_merge_sort_from_log(sorted, n, ops, ops_count, bitmap);
    printf("Reconstructed original from partial log:\n");
    for (uint32_t i = 0; i < n; ++i)
        printf("%u ", sorted[i]);
    printf("\nMatch original? %s\n", (memcmp(original, sorted, n) == 0) ? "YES" : "NO");

    free(bitmap);
    free(ops);
    free(sorted);

    return 0;
}