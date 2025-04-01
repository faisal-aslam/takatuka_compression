// main.c
#include "common_types.h"
#include "weighted_freq.h"
#include "second_pass/second_pass.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <binary_file>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    
    // First pass: Frequency analysis and sequence selection
    initializeHashTable();
    processFileInBlocks(filename);
    buildMinHeap();

    BinarySequence **topSequences = malloc(MAX_NUMBER_OF_SEQUENCES * sizeof(BinarySequence *));
    if (!topSequences) {
        perror("Failed to allocate topSequences");
        cleanupHashTable();
        cleanupHeap();
        return 1;
    }
    
    extractTopSequences(topSequences);
    
    printTopSequences(topSequences);
    
    // Test lookup
    if (MAX_NUMBER_OF_SEQUENCES > 0 && topSequences[0] != NULL) {
        BinarySequence *found = lookupSequence(topSequences[0]->sequence, topSequences[0]->length);
        if (found) {
            printf("\nLookup test successful for first sequence!\n");
        } else {
            printf("\nLookup test failed for first sequence!\n");
        }
    }

    // Cleanup first pass data structures
    for (int i = 0; i < MAX_NUMBER_OF_SEQUENCES; i++) {
        if (topSequences[i]) {
            free(topSequences[i]->sequence);
            free(topSequences[i]);
        }
    }
    free(topSequences);
    
    cleanupHashTable();  // Clean hash table (no longer needed)
    cleanupHeap();       // Clean heap (no longer needed)
    
    // Second pass: Actual compression using the selected sequences
    // Note: sequenceMap is preserved and still contains our selected sequences
    processSecondPass(filename);
    
    // Final cleanup - sequence map is only needed during second pass
    freeSequenceMap();
    
    return 0;
}
