#ifndef WEIGHTED_FREQ_H
#define WEIGHTED_FREQ_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define SEQ_LENGTH_START 2
#define SEQ_LENGTH_LIMIT 4
#define HASH_TABLE_SIZE 1000003
#define BLOCK_SIZE (1 << 20)
#define LEAST_REDUCTION 8

// Group definitions
#define GROUP1_THRESHOLD 16
#define GROUP2_THRESHOLD 32
#define GROUP3_THRESHOLD 48
#define GROUP4_THRESHOLD 4144
#define MAX_NUMBER_OF_SEQUENCES 4144

// Codeword sizes
#define GROUP1_CODE_SIZE 7
#define GROUP2_CODE_SIZE 7
#define GROUP3_CODE_SIZE 7
#define GROUP4_CODE_SIZE 15

typedef struct {
    uint8_t *sequence;
    int length;
    int count;
    int frequency;
    long potential_savings;
    int group;
} BinarySequence;

// From hash_table.h
void initializeHashTable();
void updateHashTable(uint8_t *sequence, int length);
void cleanupHashTable();

// From sequence_map.h
void initializeSequenceMap();
void freeSequenceMap();
BinarySequence* lookupSequence(uint8_t *sequence, int length);

// From heap.h
void insertIntoMinHeap(BinarySequence *seq, int m);
void buildMinHeap(int m);
void extractTopSequences(int m, BinarySequence **result);
void cleanupHeap();

// From file_processor.h
void processFileInBlocks(const char *filename, int m);
void processBlock(uint8_t *block, long blockSize, uint8_t *overlapBuffer, int *overlapSize);

// Utility functions
unsigned int fnv1a_hash(uint8_t *sequence, int length);
int compareSequences(uint8_t *seq1, uint8_t *seq2, int length);
void printTopSequences(BinarySequence **topSequences, int m);

#endif
