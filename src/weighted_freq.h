// weighted_freq.h
#ifndef WEIGHTED_FREQ_H
#define WEIGHTED_FREQ_H

#include "common_types.h"
#include "sequence_map.h" 
#include "constants.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define HASH_TABLE_SIZE 1000003




// From hash_table.h
void initializeHashTable();
void updateHashTable(uint8_t *sequence, int length);
void cleanupHashTable();

// From sequence_map.h
void initializeSequenceMap();
void freeSequenceMap();
BinarySequence* lookupSequence(uint8_t *sequence, int length);

// From heap.h
void insertIntoMinHeap(BinarySequence *seq);
void buildMinHeap();
void extractTopSequences(BinarySequence **result);
void cleanupHeap();

// From file_processor.h
void processFileInBlocks(const char *filename);

// Utility functions
unsigned int fnv1a_hash(uint8_t *sequence, int length);
void printTopSequences(BinarySequence **topSequences);

#endif
