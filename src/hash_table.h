#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include "weighted_freq.h"

typedef struct HashEntry {
    BinarySequence *seq;
    struct HashEntry *next;
} HashEntry;

void initializeHashTable();
void updateHashTable(uint8_t *sequence, int length);
void cleanupHashTable();

#endif
