#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Merge operation metadata ----
   For each merge we record left, mid, right and the bit-range [bit_start, bit_len]
   so reverse can iterate operations in reverse order and consume that exact
   bit-range in reverse.
*/
typedef struct {
    uint32_t left;
    uint32_t mid;
    uint32_t right;
    uint32_t bit_start; // first bit index (0-based) for this merge
    uint32_t bit_len;   // number of bits written for this merge
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

/* Write one bit, advance bitpos */
static inline void bw_write(uint8_t *bitmap, uint32_t *bitpos, int bit) {
    bit_set(bitmap, *bitpos, bit);
    (*bitpos)++;
}

/* ---- Forward merge that logs bits and records op metadata ---- */
static void merge_with_log(uint8_t *arr, uint8_t *temp, uint32_t left, uint32_t mid, uint32_t right, uint8_t *bitmap,
                           uint32_t *bitpos, MergeOp *ops, uint32_t *ops_count, uint32_t max_ops) {
    uint32_t start_bit = *bitpos;

    uint32_t i = left;
    uint32_t j = mid + 1;
    uint32_t k = left;

    while (i <= mid && j <= right) {
        if (arr[i] <= arr[j]) {
            temp[k++] = arr[i++];
            bw_write(bitmap, bitpos, 0);
        } else {
            temp[k++] = arr[j++];
            bw_write(bitmap, bitpos, 1);
        }
    }
    while (i <= mid)
        temp[k++] = arr[i++];
    while (j <= right)
        temp[k++] = arr[j++];

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

/* Forward iterative bottom-up merge sort with logging into ops[] and bitmap.
   Returns total bits used and sets ops_count.
   bitmap must have space for at least bitmap_bytes bytes.
   ops should be sized to at least n (safe upper bound).
*/
uint32_t merge_sort_with_bitmap_full(uint8_t *arr, uint32_t n, uint8_t *bitmap, uint32_t bitmap_bytes, MergeOp *ops,
                                     uint32_t *ops_count, uint32_t max_ops) {
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
    *ops_count = 0;

    for (uint32_t size = 1; size < n; size <<= 1) {
        for (uint32_t left = 0; left + size < n; left += 2 * size) {
            uint32_t mid = left + size - 1;
            uint32_t right = (left + 2 * size - 1 < n - 1) ? left + 2 * size - 1 : n - 1;
            if (mid < right) {
                /* bounds check for bitmap bytes: conservative check */
                uint32_t remaining_bits_capacity = bitmap_bytes * 8 - bitpos;
                if (remaining_bits_capacity == 0) {
                    fprintf(stderr, "bitmap too small\n");
                    free(temp);
                    exit(1);
                }
                merge_with_log(arr, temp, left, mid, right, bitmap, &bitpos, ops, ops_count, max_ops);
            }
        }
    }

    free(temp);
    return bitpos;
}

/* ---- Reverse a single merge using the metadata (consumes bits for that merge in reverse order) ----
   The merged subarray (arr[left..right]) is expected to hold the merged (sorted) result.
   We will reconstruct original left and right subarrays and write them back into arr[left..right].
   We consume bits from bitmap starting at bit_start for bit_len bits, but in reverse order.
*/
static void reverse_merge_using_op(uint8_t *arr, uint8_t *temp, const MergeOp *op, const uint8_t *bitmap) {
    uint32_t left = op->left;
    uint32_t mid = op->mid;
    uint32_t right = op->right;
    uint32_t start = op->bit_start;
    uint32_t len = op->bit_len;

    // Copy merged subarray (sorted result)
    memcpy(temp + left, arr + left, (right - left + 1));

    uint32_t i = left;    // write pointer for left original subarray
    uint32_t j = mid + 1; // write pointer for right original subarray

    // Reconstruct left/right subarrays using bits in forward order
    for (uint32_t t = 0; t < len; ++t) {
        int bit = bit_get(bitmap, start + t);
        if (bit == 0) {
            arr[i++] = temp[left + t]; // went to left side originally
        } else {
            arr[j++] = temp[left + t]; // went to right side originally
        }
    }

    // Copy leftover elements
    while (i <= mid) {
        arr[i] = temp[left + (i - left) + (j - (mid + 1))];
        i++;
    }
    while (j <= right) {
        arr[j] = temp[left + (i - left) + (j - (mid + 1))];
        j++;
    }
}

/* Reverse the entire merge sequence by walking ops[] backwards */
void reverse_merge_sort_from_log(uint8_t *arr, uint32_t n, const MergeOp *ops, uint32_t ops_count,
                                 const uint8_t *bitmap) {
    if (n <= 1) return;
    uint8_t *temp = malloc(n);
    if (!temp) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }

    for (int idx = (int)ops_count - 1; idx >= 0; --idx) {
        reverse_merge_using_op(arr, temp, &ops[idx], bitmap);
    }
    free(temp);
}

/* ---- Partial/limited merge sort that stops after max_comparisons but still logs ops ----
   This function logs each merge op like the full sorter (including bit_len)
   and stops merging early when comparisons >= max_comparisons.
   It returns total bits used (same as final bitpos) and ops_count filled similarly.
*/
uint32_t partial_merge_sort_with_log(uint8_t *arr, uint32_t n, uint8_t *bitmap, uint32_t bitmap_bytes, MergeOp *ops,
                                     uint32_t *ops_count, uint32_t max_ops, uint32_t max_comparisons) {
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
    *ops_count = 0;
    uint32_t comparisons = 0;

    for (uint32_t size = 1; size < n && comparisons < max_comparisons; size <<= 1) {
        for (uint32_t left = 0; left + size < n && comparisons < max_comparisons; left += 2 * size) {
            uint32_t mid = left + size - 1;
            uint32_t right = (left + 2 * size - 1 < n - 1) ? left + 2 * size - 1 : n - 1;
            if (mid < right) {
                uint32_t start_bit = bitpos;
                uint32_t i = left, j = mid + 1, k = left;
                while (i <= mid && j <= right && comparisons < max_comparisons) {
                    if (arr[i] <= arr[j]) {
                        temp[k++] = arr[i++];
                        bw_write(bitmap, &bitpos, 0);
                    } else {
                        temp[k++] = arr[j++];
                        bw_write(bitmap, &bitpos, 1);
                    }
                    comparisons++;
                }
                while (i <= mid)
                    temp[k++] = arr[i++];
                while (j <= right)
                    temp[k++] = arr[j++];
                memcpy(arr + left, temp + left, (right - left + 1));
                /* record op */
                if (*ops_count < max_ops) {
                    ops[*ops_count].left = left;
                    ops[*ops_count].mid = mid;
                    ops[*ops_count].right = right;
                    ops[*ops_count].bit_start = start_bit;
                    ops[*ops_count].bit_len = bitpos - start_bit;
                    (*ops_count)++;
                } else {
                    fprintf(stderr, "merge op array overflow\n");
                    free(temp);
                    exit(1);
                }
            }
        }
    }

    free(temp);
    return bitpos;
}

/* ---- Demo and tests ---- */
int main(void) {
    uint8_t original[] = {5, 2, 9, 1, 6, 3, 13, 4, 5, 0, 1, 19, 20};
    uint32_t n = sizeof(original) / sizeof(original[0]);

    printf("Original array:\n");
    for (uint32_t i = 0; i < n; ++i)
        printf("%u ", original[i]);
    printf("\n\n");

    /* Prepare storage: bitmap and ops array (ops sized to n is safe) */
    uint32_t bitmap_bytes = ((n * 32) + 7) / 8; /* generous upper bound */
    uint8_t *bitmap = calloc(bitmap_bytes, 1);
    MergeOp *ops = malloc(sizeof(MergeOp) * (n * 2 + 10)); /* safety margin */
    uint32_t ops_count = 0;

    /* Test 1: full merge sort with logging */
    uint8_t *sorted = malloc(n);
    memcpy(sorted, original, n);
    uint32_t used_bits = merge_sort_with_bitmap_full(sorted, n, bitmap, bitmap_bytes, ops, &ops_count, n * 2 + 10);
    printf("Sorted array:\n");
    for (uint32_t i = 0; i < n; ++i)
        printf("%u ", sorted[i]);
    printf("\nUsed bits: %u, ops_count: %u\n", used_bits, ops_count);

    /* Reverse */
    reverse_merge_sort_from_log(sorted, n, ops, ops_count, bitmap);
    printf("Reconstructed original:\n");
    for (uint32_t i = 0; i < n; ++i)
        printf("%u ", sorted[i]);
    printf("\nMatch original? %s\n\n", (memcmp(original, sorted, n) == 0) ? "YES" : "NO");

    free(sorted);
    free(bitmap);
    free(ops);

    /* Test 2: partial merge sort demonstration */
    memcpy(sorted, original, n); /* reuse small buffer */
    bitmap_bytes = ((n * 32) + 7) / 8;
    bitmap = calloc(bitmap_bytes, 1);
    ops = malloc(sizeof(MergeOp) * (n * 2 + 10));
    ops_count = 0;

    uint32_t max_comparisons = 5;
    uint32_t used_bits_partial = partial_merge_sort_with_log(sorted, n, bitmap, bitmap_bytes, ops, &ops_count,
                                                             n * 2 + 10, max_comparisons);
    printf("Partially sorted with max %u comparisons:\n", max_comparisons);
    for (uint32_t i = 0; i < n; ++i)
        printf("%u ", sorted[i]);
    printf("\nUsed bits: %u, ops_count: %u\n", used_bits_partial, ops_count);

    reverse_merge_sort_from_log(sorted, n, ops, ops_count, bitmap);
    printf("Reconstructed original from partial log:\n");
    for (uint32_t i = 0; i < n; ++i)
        printf("%u ", sorted[i]);
    printf("\nMatch original? %s\n", (memcmp(original, sorted, n) == 0) ? "YES" : "NO");

    free(bitmap);
    free(ops);

    return 0;
}
