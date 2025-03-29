#ifndef HEAP_H
#define HEAP_H

#include "weighted_freq.h"
#include "hash_table.h"      // Add this for HashEntry
#include "sequence_map.h"    // Add this for complete SequenceMapEntry

void insertIntoMinHeap(BinarySequence *seq, int m);
void buildMinHeap(int m);
void extractTopSequences(int m, BinarySequence **result, SequenceMapEntry **sequenceMap);
void cleanupHeap();
void assignGroupsByFrequency();
int compareByFrequency(const void *a, const void *b);


#endif
