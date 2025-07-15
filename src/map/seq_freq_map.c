#include "seq_freq_map.h"
#include "xxhash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

bool sequences_equal(const uint8_t *a, const uint8_t *b, uint8_t len) {
    switch (len) {
        case 1: return *a == *b;
        case 2: return *(uint16_t*)a == *(uint16_t*)b;
        case 4: return *(uint32_t*)a == *(uint32_t*)b;
        case 8: return *(uint64_t*)a == *(uint64_t*)b;
        default: return memcmp(a, b, len) == 0;
    }
}

void init_seq_freq_map(SeqFreqMap *map, uint32_t capacity) {
    map->capacity = capacity;
    map->used = 0;
    map->entries = calloc(capacity, sizeof(SeqFreqEntry));
}

void free_seq_freq_map(SeqFreqMap *map) {
    free(map->entries);
    map->entries = NULL;
    map->capacity = 0;
    map->used = 0;
}

uint32_t seq_freq_increment(SeqFreqMap *map, const uint8_t *seq, uint8_t len, uint32_t hash_index) {
    uint32_t idx = 0;
    uint64_t hash = XXH3_64bits(seq, len);
    idx = hash % map->capacity;
    uint32_t orig_idx = idx;

    while (map->entries[idx].is_used &&
           !(map->entries[idx].length == len &&
             sequences_equal(map->entries[idx].sequence, seq, len))) {
        idx = (idx + 1) % map->capacity;
        if (idx == orig_idx) {
            fprintf(stderr, "No space left in sequence frequency map\n");
            abort();
        }
    }

    if (!map->entries[idx].is_used) {
        map->entries[idx].sequence = seq;
        map->entries[idx].length = len;
        map->entries[idx].frequency = 1;
        map->entries[idx].hash = hash;
        map->entries[idx].is_used = true;
        map->used++;
        return 1;
    } else {
        return ++map->entries[idx].frequency;
    }
}

uint32_t seq_freq_set(SeqFreqMap *map, const uint8_t *seq, uint8_t len, uint32_t freq) {
    uint64_t hash = XXH3_64bits(seq, len);
    uint32_t idx = hash % map->capacity;
    uint32_t orig_idx = idx;

    while (map->entries[idx].is_used &&
           !(map->entries[idx].length == len &&
             sequences_equal(map->entries[idx].sequence, seq, len))) {
        idx = (idx + 1) % map->capacity;
        if (idx == orig_idx) {
            fprintf(stderr, "No space left in sequence frequency map\n");
            abort();
        }
    }

    if (!map->entries[idx].is_used) {
        map->entries[idx].sequence = seq;
        map->entries[idx].length = len;
        map->entries[idx].frequency = freq;
        map->entries[idx].hash = hash;
        map->entries[idx].is_used = true;
        map->used++;
        return freq;
    } else {
        map->entries[idx].frequency = freq;
        return freq;
    }
}


uint32_t seq_freq_decrement(SeqFreqMap *map, const uint8_t *seq, uint8_t len, uint32_t hash_index) {
    uint64_t hash = XXH3_64bits(seq, len);
    uint32_t idx = hash % map->capacity;
    uint32_t orig_idx = idx;

    while (map->entries[idx].is_used &&
           !(map->entries[idx].length == len &&
             sequences_equal(map->entries[idx].sequence, seq, len))) {
        idx = (idx + 1) % map->capacity;
        if (idx == orig_idx) {
            fprintf(stderr, "Sequence not found for decrement\n");
            abort();
        }
    }

    if (!map->entries[idx].is_used || map->entries[idx].frequency == 0) {
        fprintf(stderr, "Invalid decrement\n");
        abort();
    }

    map->entries[idx].frequency--;

    if (map->entries[idx].frequency == 0) {
        map->entries[idx].is_used = false;
        map->used--;
        return 0;
    }

    return map->entries[idx].frequency;
}

uint32_t seq_freq_get(const SeqFreqMap *map, const uint8_t *seq, uint8_t len, uint32_t hash_index) {
    uint64_t hash = XXH3_64bits(seq, len);
    uint32_t idx = hash % map->capacity;
    uint32_t orig_idx = idx;

    while (map->entries[idx].is_used) {
        if (map->entries[idx].length == len &&
            sequences_equal(map->entries[idx].sequence, seq, len)) {
            return map->entries[idx].frequency;
        }
        idx = (idx + 1) % map->capacity;
        if (idx == orig_idx) break;
    }

    return 0;
}
