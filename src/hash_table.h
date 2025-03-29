// hash_table.h
#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include "common_types.h"

#define HASH_TABLE_SIZE 1000003  

struct HashEntry {
    BinarySequence *seq;
    struct HashEntry *next;
};

extern HashEntry *hashTable[HASH_TABLE_SIZE];

void initializeHashTable();
void updateHashTable(uint8_t *sequence, int length);
void cleanupHashTable();

#endif
