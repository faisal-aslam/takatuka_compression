// common_types.h
#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <stdint.h>

typedef struct {
    uint8_t *sequence;
    int length;
    int count;
    int frequency;
    long potential_savings;
    int group;
} BinarySequence;

typedef struct SequenceMapEntry SequenceMapEntry;
typedef struct HashEntry HashEntry;

#endif
