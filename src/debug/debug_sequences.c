#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug_sequences.h"
#include "../constants.h"


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

    BinarySequence** sequences = calloc(MAX_NUMBER_OF_SEQUENCES, sizeof(BinarySequence*));
    if (!sequences) {
        perror("Memory allocation failed for sequences array");
        fclose(file);
        return NULL;
    }

    *count = 0;
    char line[256];

    while (*count < MAX_NUMBER_OF_SEQUENCES && fgets(line, sizeof(line), file)) {
        // === Skip comment lines and empty lines ===
        char* trimmed = line;
        while (isspace(*trimmed)) trimmed++; // Skip leading whitespace
        if (*trimmed == '#' || *trimmed == '\0' || *trimmed == '\n') {
            continue; // Skip comments and empty lines
        }

        int length, cnt, group;
        unsigned int bytes[SEQ_LENGTH_LIMIT];
        int matched;

        matched = sscanf(trimmed, "length=%d, 0x%02x, 0x%02x, count=%d, group=%d",
                         &length, &bytes[0], &bytes[1], &cnt, &group);

        if (matched == 5) {
            if (length != 2) {
                fprintf(stderr, "Warning: Length mismatch in 2-byte sequence parser\n");
                continue;
            }

            sequences[*count] = calloc(1, sizeof(BinarySequence));
            if (!sequences[*count]) {
                perror("Failed to allocate BinarySequence");
                break;
            }

            sequences[*count]->sequence = malloc(length);
            if (!sequences[*count]->sequence) {
                free(sequences[*count]);
                break;
            }

            for (int i = 0; i < length; i++) {
                sequences[*count]->sequence[i] = (uint8_t)bytes[i];
            }

            sequences[*count]->length = length;
            sequences[*count]->count = cnt;
            sequences[*count]->frequency = length * cnt;
            sequences[*count]->group = group;
            (*count)++;
        }
        else if ((matched = sscanf(trimmed, "length=%d, 0x%02x, 0x%02x, 0x%02x, count=%d, group=%d",
                                   &length, &bytes[0], &bytes[1], &bytes[2], &cnt, &group)) == 6) {
            if (length != 3) {
                fprintf(stderr, "Warning: Length mismatch in 3-byte sequence parser\n");
                continue;
            }

            sequences[*count] = calloc(1, sizeof(BinarySequence));
            if (!sequences[*count]) {
                perror("Failed to allocate BinarySequence");
                break;
            }

            sequences[*count]->sequence = malloc(length);
            if (!sequences[*count]->sequence) {
                free(sequences[*count]);
                break;
            }

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
            fprintf(stderr, "Warning: Failed to parse line: %s", trimmed);
        }
    }

    fclose(file);
    return sequences;
}

