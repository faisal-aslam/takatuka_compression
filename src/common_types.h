// common_types.h
#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <stdint.h>

typedef struct {
    uint8_t *sequence;   // The binary sequence
    int length;          // Length of the binary sequence
    int count;
    int frequency;
    uint8_t group;       // There could be at most GROUP_MAX groups 
    uint8_t isUsed;      // 1 if the sequence was used, otherwise 0
    uint16_t codeword;   // Codeword corresponding to the sequence
} BinarySequence;



#endif
