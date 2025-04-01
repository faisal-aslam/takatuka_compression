#include "debug_sequences.h"
#include "../constants.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void free_debug_sequences(BinarySequence** sequences, int count) {
    if (!sequences) return;
    
    for (int i = 0; i < count; i++) {
        if (sequences[i]) {
            free(sequences[i]->sequence);
            free(sequences[i]);
        }
    }
    free(sequences);
}

BinarySequence** load_debug_sequences(const char* filename, int* count) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open debug sequence file");
        return NULL;
    }

    BinarySequence** sequences = malloc(MAX_NUMBER_OF_SEQUENCES * sizeof(BinarySequence*));
    *count = 0;
    char line[256];
    
    while (fgets(line, sizeof(line), file) && *count < MAX_NUMBER_OF_SEQUENCES) {
        int length, cnt, group;
        long savings;
        unsigned int bytes[SEQ_LENGTH_LIMIT];
        int matched;
        
        // Try matching different sequence lengths
        matched = sscanf(line, "length=%d, 0x%02x, 0x%02x, count=%d, potential_savings=%ld, group=%d",
                       &length, &bytes[0], &bytes[1], &cnt, &savings, &group);
        
        if (matched == 6 && length == 2) {
            sequences[*count] = malloc(sizeof(BinarySequence));
            sequences[*count]->sequence = malloc(length);
            for (int i = 0; i < length; i++) {
                sequences[*count]->sequence[i] = (uint8_t)bytes[i];
            }
            sequences[*count]->length = length;
            sequences[*count]->count = cnt;
            sequences[*count]->frequency = length * cnt;
            sequences[*count]->potential_savings = savings;
            sequences[*count]->group = group;
            (*count)++;
        }
        else if ((matched = sscanf(line, "length=%d, 0x%02x, 0x%02x, 0x%02x, count=%d, potential_savings=%ld, group=%d",
                                 &length, &bytes[0], &bytes[1], &bytes[2], &cnt, &savings, &group)) == 7 && length == 3) {
            sequences[*count] = malloc(sizeof(BinarySequence));
            sequences[*count]->sequence = malloc(length);
            for (int i = 0; i < length; i++) {
                sequences[*count]->sequence[i] = (uint8_t)bytes[i];
            }
            sequences[*count]->length = length;
            sequences[*count]->count = cnt;
            sequences[*count]->frequency = length * cnt;
            sequences[*count]->potential_savings = savings;
            sequences[*count]->group = group;
            (*count)++;
        }
    }

    fclose(file);
    return sequences;
}
