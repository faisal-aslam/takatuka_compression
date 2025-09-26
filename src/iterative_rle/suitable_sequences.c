#include <stdio.h>
#include <stdint.h>

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
    int longest_len = 0;
    *best_start = UINT8_MAX;  // Fixed: use UINT8_MAX as specified
    *best_end = UINT8_MAX;

    // Fixed: use proper array sizes
    int minDeque[n], maxDeque[n];
    int minFront = 0, minBack = -1;
    int maxFront = 0, maxBack = -1;
    int left = 0;

    for (int right = 0; right < n; right++) {
        // Maintain minDeque (increasing order)
        while (minBack >= minFront && arr[minDeque[minBack]] >= arr[right]) 
            minBack--;
        minDeque[++minBack] = right;

        // Maintain maxDeque (decreasing order)
        while (maxBack >= maxFront && arr[maxDeque[maxBack]] <= arr[right]) 
            maxBack--;
        maxDeque[++maxBack] = right;

        // Check current window and shrink if necessary
        while (left <= right) {
            uint8_t min_val = arr[minDeque[minFront]];
            uint8_t max_val = arr[maxDeque[maxFront]];
            int length = right - left + 1;
            double threshold = (double)length / density;

            // Check if current window satisfies conditions
            if (length > min_size && (max_val - min_val) < threshold) {
                // Valid window found
                if (length > longest_len) {
                    longest_len = length;
                    *best_start = left;
                    *best_end = right;
                }
                break; // Don't shrink further - we want the longest valid window
            }
            
            // If condition not satisfied, shrink from left
            // But only if we're not at the minimum size
            if (length <= min_size + 1) {
                break; // Can't shrink further without making window too small
            }
            
            // Move left pointer and update deques
            if (minDeque[minFront] == left) minFront++;
            if (maxDeque[maxFront] == left) maxFront++;
            left++;
        }
    }

    return (longest_len > 0);
}

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
    // FIXED: Use sizeof(arr)/sizeof(arr[0]) to get correct array length
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

    return 0;
}