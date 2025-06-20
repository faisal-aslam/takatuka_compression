// Cleaned-up binseq_hashmap.h for use with statically allocated map pool
#ifndef BINSEQ_HASHMAP_H
#define BINSEQ_HASHMAP_H

#include <stdint.h>
#include <stddef.h>
#include "../constants.h"

// Entry for a binary sequence -> frequency map
typedef struct {
    uint8_t* binary_sequence;  // Key part
    uint16_t length;           // Key part
    int frequency;             // Value part
    int used;
} Entry;

// Map containing statically allocated entries
typedef struct BinSeqMap {
    Entry* entries;   // Provided externally (from a pool)
    size_t capacity;  // Capacity of the entries array
    size_t size;      // Number of used entries
} BinSeqMap;

// Map operations for static use
int binseq_map_put(BinSeqMap* map,
                  const uint8_t* key_sequence, uint16_t key_length,
                  int value_frequency);

const int* binseq_map_get_frequency(const BinSeqMap* map,
                                   const uint8_t* key_sequence, uint16_t key_length);

int binseq_map_increment_frequency(BinSeqMap* map,
                                 const uint8_t* key_sequence, uint16_t key_length);

void binseq_map_reset(BinSeqMap* map); // Reset entries in-place (for reuse)

Entry* binseq_map_fast_insert(Entry* entries, size_t capacity,
                                            const uint8_t* key, uint16_t key_length,
                                            int frequency);

Entry* binseq_map_fast_lookup(Entry* entries, size_t capacity,
                                            const uint8_t* key, uint16_t key_length);

void print_hashmap(BinSeqMap *map);

#endif