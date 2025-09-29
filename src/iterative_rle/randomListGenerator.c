#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void generateRandomList(int min, int max, int length) {
    for (int i = 0; i < length; i++) {
        // Generate random number in range [min, max]
        int randomNum = (rand() % (max - min + 1)) + min;
        printf("%d", randomNum);
    }
}

int main() {
    int min, max, length, numLists;
    
    // Seed the random number generator
    srand(time(NULL));
    
    // Get user input
    printf("Enter the minimum number: ");
    scanf("%d", &min);
    
    printf("Enter the maximum number: ");
    scanf("%d", &max);
    
    printf("Enter the length of each list: ");
    scanf("%d", &length);
    
    printf("Enter the number of lists to generate: ");
    scanf("%d", &numLists);
    
    // Input validation
    if (min > max) {
        printf("Error: Minimum cannot be greater than maximum.\n");
        return 1;
    }
    
    if (length <= 0) {
        printf("Error: Length must be positive.\n");
        return 1;
    }
    
    if (numLists <= 0) {
        printf("Error: Number of lists must be positive.\n");
        return 1;
    }
    
    printf("\nGenerated lists:\n");
    printf("================\n");
    
    // Generate the specified number of lists
    for (int i = 0; i < numLists; i++) {
        printf("List %d: ", i + 1);
        generateRandomList(min, max, length);
        printf("\n");
    }
    
    return 0;
}