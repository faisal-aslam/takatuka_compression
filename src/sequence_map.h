// sequence_map.h
#ifndef SEQUENCE_MAP_H
#define SEQUENCE_MAP_H

#include "common_types.h"
#include "hash_table.h"

struct SequenceMapEntry {
    BinarySequence *sequence;
    uint8_t *key;
    int key_length;
};

extern SequenceMapEntry *sequenceMap[HASH_TABLE_SIZE];

void freeSequenceMap();
BinarySequence* lookupSequence(uint8_t *sequence, int length);

#endif
