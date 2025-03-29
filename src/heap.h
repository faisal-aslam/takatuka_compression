// heap.h
#ifndef HEAP_H
#define HEAP_H

#include "weighted_freq.h"
#include "hash_table.h"
#include "sequence_map.h" 

void insertIntoMinHeap(BinarySequence *seq, int m);
void buildMinHeap(int m);
void extractTopSequences(int m, BinarySequence **result);
void cleanupHeap();
void assignGroupsByFrequency();
int compareByFrequency(const void *a, const void *b);


#endif
