#ifndef BINSEQ_HASHMAP_H
#define BINSEQ_HASHMAP_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *binary_sequence;
    uint16_t length;
} BinSeqKey;

typedef struct {
    // Replace this with your actual value fields
    int frequency;
    float weight;
} BinSeqValue;

typedef struct BinSeqMap BinSeqMap;

BinSeqMap *binseq_map_create(size_t capacity);
void binseq_map_free(BinSeqMap *map);
int binseq_map_put(BinSeqMap *map, BinSeqKey key, BinSeqValue value);
BinSeqValue *binseq_map_get(BinSeqMap *map, BinSeqKey key);
int binseq_map_contains(BinSeqMap *map, BinSeqKey key);

#endif

