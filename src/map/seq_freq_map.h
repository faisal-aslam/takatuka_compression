// seq_freq_map.h
#ifndef SEQ_FREQ_MAP_H
#define SEQ_FREQ_MAP_H

#include <stdint.h>
#include <stdbool.h>

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

    uint32_t *freq1_indices;
    uint32_t freq1_count;

    uint64_t *hash_cache;
    uint32_t hash_cache_size;
} SeqFreqMap;

void init_seq_freq_map(SeqFreqMap *map, uint32_t capacity, uint32_t hash_cache_size);
void free_seq_freq_map(SeqFreqMap *map);

void seq_freq_increment(SeqFreqMap *map, const uint8_t *seq, uint8_t len, uint32_t hash_index);
void seq_freq_decrement(SeqFreqMap *map, const uint8_t *seq, uint8_t len, uint32_t hash_index);
uint32_t seq_freq_get(const SeqFreqMap *map, const uint8_t *seq, uint8_t len, uint32_t hash_index);
void list_freq_1_sequences(const SeqFreqMap *map, void (*callback)(const uint8_t *seq, uint8_t len));
void print_seq_freq_map(const SeqFreqMap *map);

#endif // SEQ_FREQ_MAP_H
