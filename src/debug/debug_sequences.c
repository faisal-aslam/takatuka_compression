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
    /* Open the debug sequences file for reading */
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open debug sequence file");
        return NULL;
    }

    /* Allocate memory for the sequence pointers array */
    BinarySequence** sequences = calloc(MAX_NUMBER_OF_SEQUENCES, sizeof(BinarySequence*));
    if (!sequences) {
        perror("Memory allocation failed for sequences array");
        fclose(file);
        return NULL;
    }
    
    *count = 0;
    char line[256];
    
    /* Read the file line by line until we reach MAX_NUMBER_OF_SEQUENCES */
	while (*count < MAX_NUMBER_OF_SEQUENCES && fgets(line, sizeof(line), file)) {

        int length, cnt, group;
        unsigned int bytes[SEQ_LENGTH_LIMIT];
        int matched;
        
        /* Parse sequences of length 2 (2 bytes + metadata) */
        matched = sscanf(line, "length=%d, 0x%02x, 0x%02x, count=%d, group=%d",
                       &length, &bytes[0], &bytes[1], &cnt, &group);
        
        /* Handle successfully parsed 2-byte sequences */
        if (matched == 5) {
            /* Validate sequence length before processing */
            if (length != 2) {
                fprintf(stderr, "Warning: Length mismatch in 2-byte sequence parser\n");
                continue;
            }

            /* Allocate memory for the new sequence */
            sequences[*count] = calloc(1, sizeof(BinarySequence));
            if (!sequences[*count]) {
                perror("Failed to allocate BinarySequence");
                break;
            }

            /* Store the sequence bytes */
            sequences[*count]->sequence = malloc(length);
            if (!sequences[*count]->sequence) {
                free(sequences[*count]);
                break;
            }
            
            /* Copy sequence data and metadata */
            for (int i = 0; i < length; i++) {
                sequences[*count]->sequence[i] = (uint8_t)bytes[i];
            }
            sequences[*count]->length = length;
            sequences[*count]->count = cnt;
            sequences[*count]->frequency = length * cnt;
            sequences[*count]->group = group;
            (*count)++;
        }
        /* Parse sequences of length 3 (3 bytes + metadata) */
        else if ((matched = sscanf(line, "length=%d, 0x%02x, 0x%02x, 0x%02x, count=%d, group=%d",
                                 &length, &bytes[0], &bytes[1], &bytes[2], &cnt, &group)) == 6) {
            /* Validate sequence length before processing */
            if (length != 3) {
                fprintf(stderr, "Warning: Length mismatch in 3-byte sequence parser\n");
                continue;
            }

            /* Allocate memory for the new sequence */
            sequences[*count] = calloc(1, sizeof(BinarySequence));
            if (!sequences[*count]) {
                perror("Failed to allocate BinarySequence");
                break;
            }

            /* Store the sequence bytes */
            sequences[*count]->sequence = malloc(length);
            if (!sequences[*count]->sequence) {
                free(sequences[*count]);
                break;
            }
            
            /* Copy sequence data and metadata */
            for (int i = 0; i < length; i++) {
                sequences[*count]->sequence[i] = (uint8_t)bytes[i];
            }
            sequences[*count]->length = length;
            sequences[*count]->count = cnt;
            sequences[*count]->frequency = length * cnt;
            sequences[*count]->group = group;
            (*count)++;
        }
        else {
            fprintf(stderr, "Warning: Failed to parse line: %s", line);
        }
    }

    fclose(file);
    return sequences;
}
