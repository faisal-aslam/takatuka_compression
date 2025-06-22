// binseq_hashmap.h
#ifndef BINSEQ_HASHMAP_H
#define BINSEQ_HASHMAP_H

#include <stdint.h>
#include <stddef.h>
#include "../constants.h"
#include <string.h>  
#include "xxhash.h" 
#include <stdio.h>

#define HASH_MAP_SIZE 256  // Fixed size for all hash maps
#define UNUSED_INDEX UINT16_MAX

typedef struct {
    uint8_t* binary_sequence;  // Pointer to external memory
    uint16_t length;
    int frequency;
    uint32_t last_updated_level;
    uint16_t next;  // For LRU and free lists
    uint16_t prev;
    int used;       // Track if slot is occupied
} Entry;

typedef struct {
    Entry entries[HASH_MAP_SIZE];
    uint16_t free_head;
    uint16_t lru_head;
    uint16_t lru_tail;
    size_t size;
} BinSeqMap;

// Initialization
void binseq_map_init(BinSeqMap* map);

// Core operations
int binseq_map_put(BinSeqMap* map, const uint8_t* key_sequence,
                  uint16_t key_length, int value_frequency,
                  uint32_t current_level);

int binseq_map_increment_frequency(BinSeqMap* map,
                                 const uint8_t* key_sequence,
                                 uint16_t key_length,
                                 uint32_t current_level);

const Entry* binseq_map_fast_lookup(const BinSeqMap* map,
                                  const uint8_t* key_sequence,
                                  uint16_t key_length);

// Maintenance
void binseq_map_reset(BinSeqMap* map);
void print_hashmap(const BinSeqMap* map);

#endif