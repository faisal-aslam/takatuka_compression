// sequence_range.h
#ifndef SEQUENCE_RANGE_H
#define SEQUENCE_RANGE_H

#include <stdint.h>

typedef struct SequenceRange {
    uint32_t start;
    uint32_t end;
    int valid;
} SequenceRange;

#endif