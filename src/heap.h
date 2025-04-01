// heap.h
#ifndef HEAP_H
#define HEAP_H

#include "weighted_freq.h"
#include "hash_table.h"
#include "sequence_map.h" 

void insertIntoMinHeap(BinarySequence *seq);
void buildMinHeap();
void extractTopSequences(BinarySequence **result);
void cleanupHeap();



#endif
