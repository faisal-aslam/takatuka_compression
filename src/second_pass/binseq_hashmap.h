//--> binseq_hashmap.h
#ifndef BINSEQ_HASHMAP_H
#define BINSEQ_HASHMAP_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "xxhash.h"
#define COMPRESS_SEQUENCE_LENGTH 10000 //We have to prune tree whenever we reach this length.

// Forward declaration instead of including tree_node.h
struct TreeNode;

typedef struct {
    uint8_t* binary_sequence;
    uint16_t length;
} BinSeqKey;

typedef struct {
    int frequency;
    uint32_t *seqLocation;
    uint16_t seqLocationLength;
    uint16_t seqLocationCapacity;
} BinSeqValue;

typedef struct BinSeqMap BinSeqMap;

/**
* Create a new map with specified capacity.
*/
BinSeqMap *binseq_map_create(size_t capacity);

int binseq_map_put(BinSeqMap *map, BinSeqKey key, BinSeqValue value);

BinSeqValue *binseq_map_get(BinSeqMap *map, BinSeqKey key);

/**
* Copy map from one node to another. It always increment the map capacity by 1.
*/
void copyMap(struct TreeNode* mapSource, struct TreeNode* mapTarget);

BinSeqKey create_binseq_key(const uint8_t* sequence, uint16_t length);
BinSeqValue create_binseq_value(int frequency, const uint32_t* locations, uint16_t location_count);
int binseq_value_append_location(BinSeqValue *value, uint32_t loc);

void binseq_map_print(BinSeqMap *map);

/**
* Functions to free map, value, and keys.
*/
void free_key(BinSeqKey key);
void binseq_map_free(BinSeqMap *map);
void free_binseq_value(BinSeqValue value);

#endif

