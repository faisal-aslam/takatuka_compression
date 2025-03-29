#ifndef SEQUENCE_MAP_H
#define SEQUENCE_MAP_H

#include "weighted_freq.h"

typedef struct {
    BinarySequence *sequence;
    uint8_t *key;
    int key_length;
} SequenceMapEntry;

void initializeSequenceMap();
void freeSequenceMap();
BinarySequence* lookupSequence(uint8_t *sequence, int length);

#endif
