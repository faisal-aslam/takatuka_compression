#include <stdio.h>
#include <limits.h>
#include "suitable_sequences.h"

//#define TEST_SUITABLE_SEQ
/*
Finds the longest consecutive sub-block in `arr` that satisfies:

1. Sub-block length > min_size
2. Let min_val = smallest element in the sub-block,
   and max_val = largest element in the sub-block.
   Then (max_val - min_val) < (sub-block length / density)

If found:
   - *best_start = start index
   - *best_end   = end index
Otherwise:
   - *best_start = *best_end = UINT8_MAX
*/
int find_suitable_subblock(uint8_t *arr, int n, double density,
                           int min_size, int *best_start, int *best_end) {
    int best_len = 0;
    int best_range = INT_MAX;
    *best_start = -1;
    *best_end = -1;

    for (int left = 0; left < n; left++) {
        uint8_t min_val = arr[left];
        uint8_t max_val = arr[left];

        for (int right = left; right < n; right++) {
            if (arr[right] < min_val) min_val = arr[right];
            if (arr[right] > max_val) max_val = arr[right];

            int length = right - left + 1;
            int range = max_val - min_val;
            double threshold = (double)length / density;

            if (length >= min_size && range <= threshold) {
                // Better candidate if:
                //  1) longer length, or
                //  2) same length but smaller range
                if (length > best_len ||
                    (length == best_len && range < best_range)) {
                    best_len = length;
                    best_range = range;
                    *best_start = left;
                    *best_end = right;
                }
            }
        }
    }

    return (best_len > 0);
}

#ifdef TEST_SUITABLE_SEQ
// helper: print subblock if found
void print_subblock(uint8_t *arr, int start, int end, double density) {
    uint8_t min_val = arr[start], max_val = arr[start];
    for (int i = start; i <= end; i++) {
        if (arr[i] < min_val) min_val = arr[i];
        if (arr[i] > max_val) max_val = arr[i];
    }

    int length = end - start + 1;
    double threshold = (double)length / density;

    printf("  ✅ Selected block (indices %d..%d, length %d): ",
           start, end, length);
    for (int i = start; i <= end; i++) {
        printf("%3u ", arr[i]);
    }
    printf("\n    Min=%u Max=%u Range=%u Threshold=%.2f\n",
           min_val, max_val, max_val - min_val, threshold);
}

// test harness - FIXED: divide by element size to get correct length
void run_test(uint8_t *arr, int n, double density, int min_size, const char *label) {
    printf("Test: %s\nArray: ", label);
    for (int i = 0; i < n; i++) {
        printf("%3u ", arr[i]);
    }
    printf("\n");

    int start, end;
    if (find_suitable_subblock(arr, n, density, min_size, &start, &end)) {
        print_subblock(arr, start, end, density);
    } else {
        printf("  ❌ No valid sub-block found.\n");
    }
    printf("\n");
}

int main(void) {
    /*
    uint8_t arr1[] = {100, 5, 200, 7, 250};
    run_test(arr1, sizeof(arr1)/sizeof(arr1[0]), 2.0, 2, "Random - No valid sub-block");

    uint8_t arr2[] = {5, 50, 6, 7, 100};
    run_test(arr2, sizeof(arr2)/sizeof(arr2[0]), 2.0, 2, "Random - One valid sub-block");

    uint8_t arr3[] = {10, 52, 11, 12, 50, 53, 51};
    run_test(arr3, sizeof(arr3)/sizeof(arr3[0]), 2.0, 2, "Random - Multiple valid sub-blocks");

    uint8_t arr4[] = {30, 29, 31, 32, 28};
    run_test(arr4, sizeof(arr4)/sizeof(arr4[0]), 2.0, 2, "Random - Entire array valid");

    uint8_t arr5[] = {42};
    run_test(arr5, sizeof(arr5)/sizeof(arr5[0]), 2.0, 2, "Random - Too small");

    uint8_t arr6[] = {
        'k','k','k','k','k','k','k','k','e','f','g','k','k','k','k','k','k','k',
        'k','k','k','e','g','h','i','a','f','j','k','b','c','d','e'
    };
    run_test(arr6, sizeof(arr6)/sizeof(arr6[0]), 2.0, 2, "Random - One valid long sub-block");

    uint8_t arr7[256];
    for (int i = 0; i < 256; i++) {
        if (i == 0)        arr7[i] = 0;    // extreme low outlier
        else if (i == 255) arr7[i] = 200;  // extreme high outlier
        else               arr7[i] = 50 + (i % 10); // values 50..59
    }
    run_test(arr7, 256, 2.0, 16, "256-length array with valid sub-block only in the middle");
    */
    uint8_t arr8[]={255, 190, 1, 2, 3,4, 5, 6, 7, 7,7,7,7,7,7,7,7,7, 210, 214, 220, 230, 222, 22, 20, 198  };    
    run_test(arr8, sizeof(arr8)/sizeof(arr8[0]), 2.0, 2, "it should not select all in this case");

    return 0;
}

#endif