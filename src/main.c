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
    topSequences = load_debug_sequences(DEBUG_SEQUENCE_FILE, &sequenceCount);
    if (!topSequences) {
        fprintf(stderr, "Debug load failed, falling back to normal processing\n");
        // Continue with normal processing
        initializeHashTable();
        processFileInBlocks(argv[1]);
        buildMinHeap();
        topSequences = malloc(MAX_NUMBER_OF_SEQUENCES * sizeof(BinarySequence *));
        extractTopSequences(topSequences);
        sequenceCount = MAX_NUMBER_OF_SEQUENCES;
    } else {
        // Initialize sequence map with debug sequences
        initializeSequenceMap();
        for (int i = 0; i < sequenceCount; i++) {
            if (topSequences[i]) {
                unsigned int hashValue = fnv1a_hash(topSequences[i]->sequence, 
                                                  topSequences[i]->length);
                SequenceMapEntry *entry = calloc(1, sizeof(SequenceMapEntry));
                entry->key = malloc(topSequences[i]->length);
                if (!entry->key) {
                    free(entry);
                    continue;
                }
                memcpy(entry->key, topSequences[i]->sequence, topSequences[i]->length);
                entry->key_length = topSequences[i]->length;
                entry->sequence = topSequences[i];
                sequenceMap[hashValue] = entry;
            }
        }
    }
#else
    // Normal processing path
    initializeHashTable();
    processFileInBlocks(argv[1]);
    buildMinHeap();
    topSequences = malloc(MAX_NUMBER_OF_SEQUENCES * sizeof(BinarySequence *));
    extractTopSequences(topSequences);
    sequenceCount = MAX_NUMBER_OF_SEQUENCES;
#endif


#ifdef DEBUG
print_results:
#endif
    printTopSequences(topSequences);

    processSecondPass(argv[1], topSequences);

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
