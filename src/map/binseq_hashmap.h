// binseq_hashmap.h
#ifndef BINSEQ_HASHMAP_H
#define BINSEQ_HASHMAP_H

#include <stdint.h>
#include <stddef.h>
#include "../constants.h"
#include <string.h>  
#include "xxhash.h" 

#define HASH_MAP_SIZE 256  // Fixed size for all hash maps

// Entry for a binary sequence -> frequency map
typedef struct {
    uint8_t* binary_sequence;  // Key part (pointer to external memory)
    uint16_t length;           // Key part
    int frequency;             // Value part
    int used;
} Entry;

// Map containing fixed-size entries
typedef struct BinSeqMap {
    Entry entries[HASH_MAP_SIZE];  // Static array of entries
    size_t size;      // Number of used entries
} BinSeqMap;


static inline uint64_t fast_hash(const uint8_t* key, uint16_t length) {
    if (length <= 4) {
        uint32_t hash = 0;
        memcpy(&hash, key, length);
        return hash;
    }
    return XXH3_64bits(key, length);
}

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

int binseq_map_resize(BinSeqMap* map, size_t min_new_capacity);

void print_hashmap(BinSeqMap *map);

#endif