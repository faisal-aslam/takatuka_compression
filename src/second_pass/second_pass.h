// src/second_pass/second_pass.h
#ifndef SECOND_PASS_H
#define SECOND_PASS_H

//#define DEBUG_PRINT 1

#include <limits.h>
#include "../weighted_freq.h"
#include "tree_node.h"


#define MAX_TREE_NODES COMPRESS_SEQUENCE_LENGTH
#define SAVING_GAP SEQ_LENGTH_LIMIT*10

void processSecondPass(const char* filename);
void processBlockSecondPass(const uint8_t* block, uint32_t blockSize);

#endif
