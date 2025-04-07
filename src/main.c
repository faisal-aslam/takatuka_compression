#include "common_types.h"
#include "weighted_freq.h"
#include "second_pass/second_pass.h"

#ifdef DEBUG
#include "debug/debug_sequences.h"
#define DEBUG_SEQUENCE_FILE "src/debug/debug_sequences.txt"
#endif

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <binary_file>\n", argv[0]);
        return 1;
    }

    // Initialize common components
    initializeHashTable();
    initializeSequenceMap();
    
    BinarySequence **topSequences;
    int sequenceCount = 0;

#ifdef DEBUG
    // DEBUG path - try to load from file first
    topSequences = load_debug_sequences(DEBUG_SEQUENCE_FILE, &sequenceCount);
    if (!topSequences) {
        fprintf(stderr, "Debug load failed, falling back to normal processing\n");
        // Fall through to normal processing
#endif
    // Normal processing path (used in both DEBUG fallthrough and non-DEBUG)
    if (!topSequences) {
        processFileInBlocks(argv[1]);
        buildMinHeap();
        topSequences = malloc(MAX_NUMBER_OF_SEQUENCES * sizeof(BinarySequence *));
        extractTopSequences(topSequences);
        sequenceCount = MAX_NUMBER_OF_SEQUENCES;
    }
#ifdef DEBUG
    }
#endif

    printTopSequences(topSequences);
    processSecondPass(argv[1], topSequences);

    // Common cleanup
    for (int i = 0; i < sequenceCount; i++) {
        if (topSequences[i]) {
            free(topSequences[i]->sequence);
            free(topSequences[i]);
        }
    }
    free(topSequences);
    
    cleanupHashTable();
    freeSequenceMap();
    return 0;
}

