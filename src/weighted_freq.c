// weighted_freq.c
#include "weighted_freq.h"
#include <string.h>

// FNV-1a hash function implementation
unsigned int fnv1a_hash(uint8_t *sequence, int length) {
    const unsigned int FNV_prime = 16777619;
    unsigned int hash = 2166136261;  // FNV offset basis

    for (int i = 0; i < length; i++) {
        hash ^= sequence[i];
        hash *= FNV_prime;
    }

    return hash % HASH_TABLE_SIZE;
}

// Function to print the top sequences
void printTopSequences(BinarySequence **topSequences) {
    long total_savings = 0;
    for (int i = 0; i < MAX_NUMBER_OF_SEQUENCES; i++) {
        if (topSequences[i] == NULL) {
            //printf("%d: [EMPTY SLOT]\n", i);
            continue;
        }
        
        printf("%d: Sequence: ", i);
        for (int j = 0; j < topSequences[i]->length; j++) {
            printf("%02X ", topSequences[i]->sequence[j]);
        }
        printf("Count: %d, Freq: %d, Group: %d, Savings: %ld bits\n",
               topSequences[i]->count,
               topSequences[i]->frequency,
               topSequences[i]->group,
               topSequences[i]->potential_savings);
       if (topSequences[i]->group<=4) {
           total_savings += topSequences[i]->potential_savings;
       }
    }
    printf("Total Savings =%ld", total_savings);
}
