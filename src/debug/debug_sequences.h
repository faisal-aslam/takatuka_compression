#ifndef DEBUG_SEQUENCES_H
#define DEBUG_SEQUENCES_H

#include "../common_types.h"

BinarySequence** load_debug_sequences(const char* filename, int* count);
void free_debug_sequences(BinarySequence** sequences, int count);

#endif
