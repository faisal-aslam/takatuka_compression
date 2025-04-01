#include "debug_sequences.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../constants.h"

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
        uint8_t bytes[SEQ_LENGTH_LIMIT];
        int byteCount = 0;
        char* token = strtok(line, ",");
        
        while (token && byteCount < SEQ_LENGTH_LIMIT) {
            unsigned int byte;
            if (sscanf(token, "0x%02x", &byte) == 1) {
                bytes[byteCount++] = (uint8_t)byte;
            }
            token = strtok(NULL, ",");
        }

        if (byteCount >= SEQ_LENGTH_START) {
            sequences[*count] = malloc(sizeof(BinarySequence));
            sequences[*count]->sequence = malloc(byteCount);
            memcpy(sequences[*count]->sequence, bytes, byteCount);
            sequences[*count]->length = byteCount;
            sequences[*count]->count = 1;
            sequences[*count]->frequency = byteCount;
            sequences[*count]->group = 1;
            sequences[*count]->potential_savings = (byteCount * 8 - GROUP1_CODE_SIZE) * 1;
            (*count)++;
        }
    }

    fclose(file);
    return sequences;
}

void free_debug_sequences(BinarySequence** sequences, int count) {
    for (int i = 0; i < count; i++) {
        if (sequences[i]) {
            free(sequences[i]->sequence);
            free(sequences[i]);
        }
    }
    free(sequences);
}
