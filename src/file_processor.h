#ifndef FILE_PROCESSOR_H
#define FILE_PROCESSOR_H

#include "weighted_freq.h"

void processFileInBlocks(const char *filename);  
void processBlock(uint8_t *block, long blockSize, uint8_t *overlapBuffer, int *overlapSize);

#endif
