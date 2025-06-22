// binseq_hashmap.c
#include "binseq_hashmap.h"
#include <string.h>
#include "xxhash.h"
#include <stdio.h>

// Helper: hash a binary sequence
static uint64_t hash_sequence(const uint8_t* sequence, uint16_t length) {
    if (!sequence || length == 0) return 0;
    return XXH3_64bits(sequence, length);
}

// Helper: compare sequences
static int sequences_equal(const uint8_t* a, uint16_t a_len,
                           const uint8_t* b, uint16_t b_len) {
    if (a_len != b_len) return 0;
    return memcmp(a, b, a_len) == 0;
}

// Helper: find entry
static Entry* find_entry(BinSeqMap* map,
                         const uint8_t* sequence, uint16_t length) {
    if (!map || !sequence || length == 0)
        return NULL;  // Defensive guard

    uint64_t hash = hash_sequence(sequence, length);
    size_t index = hash % HASH_MAP_SIZE;

    for (size_t i = 0; i < HASH_MAP_SIZE; ++i) {
        size_t try = (index + i) % HASH_MAP_SIZE;
        Entry* entry = &map->entries[try];

        if (!entry->used) return NULL;
        if (sequences_equal(entry->binary_sequence, entry->length, sequence, length)) {
            return entry;
        }
    }
    return NULL;
}

int binseq_map_put(BinSeqMap* map,
                   const uint8_t* key_sequence, uint16_t key_length,
                   int value_frequency) {
    if (!map || !key_sequence || key_length == 0) return 0;

    Entry* existing = find_entry(map, key_sequence, key_length);
    if (existing) {
        existing->frequency = value_frequency;
        return 1;
    }

    if (map->size >= HASH_MAP_SIZE) {
        return 0; // Map is full
    }

    uint64_t hash = hash_sequence(key_sequence, key_length);
    size_t index = hash % HASH_MAP_SIZE;

    for (size_t i = 0; i < HASH_MAP_SIZE; ++i) {
        size_t try = (index + i) % HASH_MAP_SIZE;
        Entry* entry = &map->entries[try];

        if (!entry->used) {
            entry->binary_sequence = (uint8_t*)key_sequence; // assume static or caller-managed
            entry->length = key_length;
            entry->frequency = value_frequency;
            entry->used = 1;
            map->size++;
            return 1;
        }
    }

    return 0; // no space (shouldn't happen if size < HASH_MAP_SIZE)
}

const int* binseq_map_get_frequency(const BinSeqMap* map,
                                    const uint8_t* key_sequence, uint16_t key_length) {
    Entry* entry = find_entry((BinSeqMap*)map, key_sequence, key_length);
    return entry ? &entry->frequency : NULL;
}

int binseq_map_increment_frequency(BinSeqMap* map,
                                 const uint8_t* key_sequence, uint16_t key_length) {
    if (!map || !key_sequence || key_length == 0) return 0;

    uint64_t hash = hash_sequence(key_sequence, key_length);
    size_t index = hash % HASH_MAP_SIZE;

    for (size_t i = 0; i < HASH_MAP_SIZE; ++i) {
        Entry* entry = &map->entries[index];
        
        if (entry->used) {
            if (entry->length == key_length && 
                memcmp(entry->binary_sequence, key_sequence, key_length) == 0) {
                entry->frequency++;
                return 1;
            }
        } else {
            // Found empty slot - insert
            if (map->size >= HASH_MAP_SIZE) {
                return 0; // Map is full
            }
            entry->binary_sequence = (uint8_t*)key_sequence;
            entry->length = key_length;
            entry->frequency = 1;
            entry->used = 1;
            map->size++;
            return 1;
        }
        
        index = (index + 1) % HASH_MAP_SIZE;
    }

    return 0; // Map is full
}

void binseq_map_reset(BinSeqMap* map) {
    if (!map) return;

    for (size_t i = 0; i < HASH_MAP_SIZE; ++i) {
        Entry* entry = &map->entries[i];
        entry->binary_sequence = NULL;
        entry->length = 0;
        entry->frequency = 0;
        entry->used = 0;
    }

    map->size = 0;
}

Entry* binseq_map_fast_lookup(Entry* entries, size_t capacity,
                            const uint8_t* key, uint16_t key_length) {
    if (!entries || capacity == 0 || !key || key_length == 0) 
        return NULL;

    uint64_t hash = XXH3_64bits(key, key_length);
    size_t index = hash % capacity;

    for (size_t i = 0; i < capacity; ++i) {
        Entry* e = &entries[index];
        if (!e->used) return NULL;
        if (e->length == key_length && 
            memcmp(e->binary_sequence, key, key_length) == 0) {
            return e;
        }
        index = (index + 1) % capacity;
    }

    return NULL;
}

Entry* binseq_map_fast_insert(Entry* entries, size_t capacity,
                            const uint8_t* key, uint16_t key_length,
                            int frequency) {
    if (!entries || capacity == 0 || !key || key_length == 0) return NULL;

    uint64_t hash = XXH3_64bits(key, key_length);
    size_t index = hash % capacity;

    for (size_t i = 0; i < capacity; ++i) {
        size_t probe = (index + i) % capacity;
        Entry* e = &entries[probe];

        if (!e->used) {
            e->binary_sequence = (uint8_t*)key;
            e->length = key_length;
            e->frequency = frequency;
            e->used = 1;
            return e;
        }
    }

    return NULL;
}

void print_hashmap(BinSeqMap *map) {
    if (!map) {
        printf("Hashmap is NULL.\n");
        return;
    }

    printf("Hashmap contents (size = %zu, capacity = %d):\n", map->size, HASH_MAP_SIZE);

    for (size_t i = 0; i < HASH_MAP_SIZE; ++i) {
        Entry* e = &map->entries[i];

        if (!e->used)
            continue;

        printf("  [%zu] Length: %u, Frequency: %d, Sequence: ", i, e->length, e->frequency);
        for (uint16_t j = 0; j < e->length; ++j) {
            printf("%02X ", e->binary_sequence[j]);
        }
        printf("\n");
    }

    if (map->size == 0) {
        printf("  (empty)\n");
    }
}