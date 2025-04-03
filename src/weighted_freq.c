// weighted_freq.c
#include "weighted_freq.h"
#include <string.h>
#include <limits.h>

int groupCodeSize(int group) {
	switch(group) {
		case 1: return GROUP1_CODE_SIZE;
		case 2: return GROUP2_CODE_SIZE;
		case 3: return GROUP3_CODE_SIZE;
		case 4: return GROUP4_CODE_SIZE;
		default: return INT_MAX;
	}
}


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
    //printf("\n print top seq is called \n");
    for (int i = 0; i < MAX_NUMBER_OF_SEQUENCES; i++) {
        if (topSequences[i] == NULL) {
            //printf("%d: [EMPTY SLOT]\n", i);
            continue;
        }
        
        printf("%d: Sequence: ", i);
        for (int j = 0; j < topSequences[i]->length; j++) {
            printf("%02X ", topSequences[i]->sequence[j]);
        }
        printf("Count(C): %d, Length(L): %d, Freq(C*L):  %d, Group: %d, Savings (L*8-GL)*C: %d bits\n",
               topSequences[i]->count,
               topSequences[i]->length,
               topSequences[i]->frequency,
               topSequences[i]->group,
               ((topSequences[i]->length*8-groupCodeSize(topSequences[i]->group))*topSequences[i]->count));
    }
    
}
