//sp_sf_map.h

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "xxhash.h"
#include "constants.h"

typedef struct {
    const uint8_t *sequence;  // external pointer (non-owning)
    uint8_t length;           //length of the sequence
    uint32_t freq;            //frequency
} SFEntry;

typedef struct SeqFreqMap {
    SFEntry *entries;
} SFMap;

void init_sf_map(uint16_t capacity);
uint32_t sf_decrement(const uint8_t *seq, uint8_t len);
uint32_t sf_increment(const uint8_t *seq, uint8_t len);
uint32_t sf_set(const uint8_t *seq, uint8_t len, uint32_t freq);
uint32_t sf_get(const uint8_t *seq, uint8_t len);
void sf_map_print();