#ifndef BINSEQ_HASHMAP_H
#define BINSEQ_HASHMAP_H

#include <stdint.h>
#include <stddef.h>
#include "xxhash.h"
#define COMPRESS_SEQUENCE_LENGTH 10000 //We have to prune tree whenever we reach this length. 

// Forward declaration instead of including tree_node.h
struct TreeNode;

typedef struct {
    uint8_t *binary_sequence;
    uint16_t length;
} BinSeqKey;

typedef struct {
    int frequency;
    uint32_t seqLocation[COMPRESS_SEQUENCE_LENGTH];
    uint16_t seqLocationLength;
} BinSeqValue;

typedef struct BinSeqMap BinSeqMap;

BinSeqMap *binseq_map_create(size_t capacity);
void binseq_map_free(BinSeqMap *map);
int binseq_map_put(BinSeqMap *map, BinSeqKey key, BinSeqValue value);
BinSeqValue *binseq_map_get(BinSeqMap *map, BinSeqKey key);
int binseq_map_contains(BinSeqMap *map, BinSeqKey key);
void copyMap(struct TreeNode* mapSource, struct TreeNode* mapTarget, int increaseSize);

#endif

