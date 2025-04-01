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

    BinarySequence **topSequences;
    int sequenceCount = 0;

#ifdef DEBUG
    // Debug mode - load from file
    topSequences = load_debug_sequences(DEBUG_SEQUENCE_FILE, &sequenceCount);
    if (!topSequences) {
        fprintf(stderr, "Debug load failed, falling back to normal processing\n");
        // Fall through to normal processing
    } else {
        // Initialize sequence map with debug sequences
        initializeSequenceMap();
        for (int i = 0; i < sequenceCount; i++) {
            if (topSequences[i]) {
                unsigned int hashValue = fnv1a_hash(topSequences[i]->sequence, 
                                                  topSequences[i]->length);
                SequenceMapEntry *entry = malloc(sizeof(SequenceMapEntry));
                entry->key = malloc(topSequences[i]->length);
                memcpy(entry->key, topSequences[i]->sequence, topSequences[i]->length);
                entry->key_length = topSequences[i]->length;
                entry->sequence = topSequences[i];
                sequenceMap[hashValue] = entry;
            }
        }
        goto print_results;  // Skip normal processing
    }
#endif

    // Normal processing path
    initializeHashTable();
    processFileInBlocks(argv[1]);
    buildMinHeap();

    topSequences = malloc(MAX_NUMBER_OF_SEQUENCES * sizeof(BinarySequence *));
    extractTopSequences(topSequences);
    sequenceCount = MAX_NUMBER_OF_SEQUENCES;

#ifdef DEBUG
print_results:
#endif
    printTopSequences(topSequences);

    processSecondPass(argv[1]);

    // Cleanup
#ifdef DEBUG
    free_debug_sequences(topSequences, sequenceCount);
#else
    for (int i = 0; i < sequenceCount; i++) {
        if (topSequences[i]) {
            free(topSequences[i]->sequence);
            free(topSequences[i]);
        }
    }
    free(topSequences);
#endif
    
    freeSequenceMap();
    return 0;
}
