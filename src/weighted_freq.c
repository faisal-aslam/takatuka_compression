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

// Function to compare two binary sequences
int compareSequences(uint8_t *seq1, uint8_t *seq2, int length) {
    if (seq1 == NULL || seq2 == NULL || length <= 0) {
        fprintf(stderr, "Error: Invalid sequence or length in compareSequences.\n");
        return -1;  // Indicate an error
    }
    return memcmp(seq1, seq2, length);
}

// Function to print the top sequences
void printTopSequences(BinarySequence **topSequences, int m) {
    long total_savings = 0;
    for (int i = 0; i < m; i++) {
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
