#ifndef SEQ_FREQ_MAP_H
#define SEQ_FREQ_MAP_H

#include <stdint.h>
#include <stdbool.h>
#include "../constants.h"

typedef struct {
    const uint8_t *sequence;  // Points to external memory
    uint8_t length;
    uint32_t frequency;
    uint64_t hash;
    bool is_used;
} SeqFreqEntry;

typedef struct {
    SeqFreqEntry *entries;
    uint32_t capacity;
    uint32_t used;
} SeqFreqMap;

void init_seq_freq_map(SeqFreqMap *map, uint32_t capacity);
void free_seq_freq_map(SeqFreqMap *map);

uint32_t seq_freq_increment(SeqFreqMap *map, const uint8_t *seq, uint8_t len, uint32_t hash_index);
uint32_t seq_freq_decrement(SeqFreqMap *map, const uint8_t *seq, uint8_t len, uint32_t hash_index);
uint32_t seq_freq_get(const SeqFreqMap *map, const uint8_t *seq, uint8_t len, uint32_t hash_index);
uint32_t seq_freq_set(SeqFreqMap *map, const uint8_t *seq, uint8_t len, uint32_t freq);
bool sequences_equal(const uint8_t *a, const uint8_t *b, uint8_t len);

#endif // SEQ_FREQ_MAP_H
