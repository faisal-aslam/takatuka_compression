// seq_freq_map.c
#include "seq_freq_map.h"
#include "xxhash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static inline bool sequences_equal(const uint8_t *a, const uint8_t *b, uint8_t len) {
    switch (len) {
        case 1: return *a == *b;
        case 2: return *(uint16_t*)a == *(uint16_t*)b;
        case 4: return *(uint32_t*)a == *(uint32_t*)b;
        case 8: return *(uint64_t*)a == *(uint64_t*)b;
        default: return memcmp(a, b, len) == 0;
    }
}

void init_seq_freq_map(SeqFreqMap *map, uint32_t capacity, uint32_t hash_cache_size) {
    map->capacity = capacity;
    map->used = 0;
    map->freq1_count = 0;

    map->entries = calloc(capacity, sizeof(SeqFreqEntry));
    map->freq1_indices = calloc(capacity, sizeof(uint32_t));
    map->hash_cache = calloc(hash_cache_size, sizeof(uint64_t));
    map->hash_cache_size = hash_cache_size;
}

void free_seq_freq_map(SeqFreqMap *map) {
    free(map->entries);
    free(map->freq1_indices);
    free(map->hash_cache);
    map->entries = NULL;
    map->freq1_indices = NULL;
    map->hash_cache = NULL;
    map->capacity = 0;
    map->used = 0;
    map->freq1_count = 0;
    map->hash_cache_size = 0;
}

void seq_freq_increment(SeqFreqMap *map, const uint8_t *seq, uint8_t len, uint32_t hash_index) {
    uint64_t hash = map->hash_cache[hash_index];
    if (hash == 0) {
        hash = XXH3_64bits(seq, len);
        map->hash_cache[hash_index] = hash;
    }

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
        map->entries[idx].frequency = 1;
        map->entries[idx].hash = hash;
        map->entries[idx].is_used = true;
        map->freq1_indices[map->freq1_count++] = idx;
        map->used++;
    } else {
        if (map->entries[idx].frequency == 1) {
            for (uint32_t i = 0; i < map->freq1_count; i++) {
                if (map->freq1_indices[i] == idx) {
                    map->freq1_indices[i] = map->freq1_indices[--map->freq1_count];
                    break;
                }
            }
        }
        map->entries[idx].frequency++;
    }
}

void seq_freq_decrement(SeqFreqMap *map, const uint8_t *seq, uint8_t len, uint32_t hash_index) {
    uint64_t hash = map->hash_cache[hash_index];
    if (hash == 0) {
        hash = XXH3_64bits(seq, len);
        map->hash_cache[hash_index] = hash;
    }

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

    if (map->entries[idx].frequency == 1) {
        map->entries[idx].is_used = false;
        map->used--;
        for (uint32_t i = 0; i < map->freq1_count; i++) {
            if (map->freq1_indices[i] == idx) {
                map->freq1_indices[i] = map->freq1_indices[--map->freq1_count];
                break;
            }
        }
    } else {
        map->entries[idx].frequency--;
        if (map->entries[idx].frequency == 1) {
            map->freq1_indices[map->freq1_count++] = idx;
        }
    }
}

uint32_t seq_freq_get(const SeqFreqMap *map, const uint8_t *seq, uint8_t len, uint32_t hash_index) {
    uint64_t hash = map->hash_cache[hash_index];
    if (hash == 0) {
        hash = XXH3_64bits(seq, len);
    }

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

void list_freq_1_sequences(const SeqFreqMap *map, void (*callback)(const uint8_t *seq, uint8_t len)) {
    for (uint32_t i = 0; i < map->freq1_count; i++) {
        uint32_t idx = map->freq1_indices[i];
        callback(map->entries[idx].sequence, map->entries[idx].length);
    }
}

void print_seq_freq_map(const SeqFreqMap *map) {
    printf("\n--- Sequence Frequency Map ---\n");
    for (uint32_t i = 0; i < map->capacity; i++) {
        if (map->entries[i].is_used) {
            printf("[%03u] len=%u freq=%u hash=%llu\n",
                   i,
                   map->entries[i].length,
                   map->entries[i].frequency,
                   (unsigned long long)map->entries[i].hash);
        }
    }
    printf("------------------------------\n\n");
}
