// weighted_freq.c
#include "weighted_freq.h"
#include <string.h>
#include <limits.h>
#include "debug/print_stack_trace.h"

/*
 *    - 3-bit length
 *    - N bytes of sequence data
 *    - 2-bit group
 *    - groupCodeSize()
*/
uint8_t getHeaderOverhead(uint8_t group, uint16_t seq_length) {
	if (group >= 0 && group < TOTAL_GROUPS) {
		return 3+seq_length+2+groupCodeSize(group);
	} else {		
        fprintf(stderr, "Invalid group %d Exiting!\n", group);
        print_stacktrace();
        exit(0);
        return 0;
    }
}

// Returns JUST the codeword bits (excluding flag + group bits)
uint8_t groupCodeSize(uint8_t group) {
    switch(group) {
        case 0: return 4;  // 4-bit codeword
        case 1: return 4;  // 4-bit codeword
        case 2: return 4;  // 4-bit codeword
        case 3: return 12; // 12-bit codeword
        default:
            fprintf(stderr, "Invalid group %d Exiting!\n", group);
            print_stacktrace();
            exit(0);
            return 0;
    }
}


//Returns overhead of a group.
uint8_t groupOverHead(uint8_t group) {
	(void)group; // Suppressing unused parameter warning. In future we might need it.
	return 3;
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
        
        long savings = (topSequences[i]->length*8-(groupCodeSize(topSequences[i]->group)+groupOverHead(topSequences[i]->group)))*topSequences[i]->count;
        printf("Count(C): %d, Length(L): %d, Freq(C*L):  %d, Group: %d, Savings (L*8-GL)*C: %ld bits\n",
               topSequences[i]->count,
               topSequences[i]->length,
               topSequences[i]->frequency,
               topSequences[i]->group,
               savings);
    }
    
}
