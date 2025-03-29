#include "common_types.h"
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
    processFileInBlocks(filename);
    
    // Build and extract sequences
    buildMinHeap(MAX_NUMBER_OF_SEQUENCES);

	SequenceMapEntry* sequenceMap[HASH_TABLE_SIZE] = {0}; 
		
    BinarySequence **topSequences = malloc(MAX_NUMBER_OF_SEQUENCES * sizeof(BinarySequence *));
    extractTopSequences(MAX_NUMBER_OF_SEQUENCES, topSequences, sequenceMap);
    
    // Print results
    printTopSequences(topSequences, MAX_NUMBER_OF_SEQUENCES);
	
	
    
    // Test the sequence lookup
    if (MAX_NUMBER_OF_SEQUENCES > 0 && topSequences[0] != NULL) {
        BinarySequence *found = lookupSequence(topSequences[0]->sequence, topSequences[0]->length);
        if (found) {
            printf("\n\nLookup test successful for first sequence!\n");
        } else {
            printf("\n\nLookup test failed for first sequence!\n");
        }
    }


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
