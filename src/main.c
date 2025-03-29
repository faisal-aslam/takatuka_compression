#include "weighted_freq.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <binary_file>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    
    // Initialize components
    initializeHashTable();
    initializeSequenceMap();
    
    // Process file
    processFileInBlocks(filename, MAX_NUMBER_OF_SEQUENCES);
    
    // Build and extract sequences
    buildMinHeap(MAX_NUMBER_OF_SEQUENCES);
    
    BinarySequence **topSequences = malloc(MAX_NUMBER_OF_SEQUENCES * sizeof(BinarySequence *));
    extractTopSequences(MAX_NUMBER_OF_SEQUENCES, topSequences);
    
    // Print results
    printTopSequences(topSequences, MAX_NUMBER_OF_SEQUENCES);
    
    // Cleanup
    for (int i = 0; i < MAX_NUMBER_OF_SEQUENCES; i++) {
        if (topSequences[i]) {
            free(topSequences[i]->sequence);
            free(topSequences[i]);
        }
    }
    free(topSequences);
    
    cleanupHashTable();
    freeSequenceMap();
    cleanupHeap();
    
    return 0;
}
