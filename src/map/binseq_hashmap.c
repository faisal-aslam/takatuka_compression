#include "binseq_hashmap.h"
#include <string.h>
#include "xxhash.h"

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
static Entry* find_entry(const BinSeqMap* map,
                         const uint8_t* sequence, uint16_t length) {
    uint64_t hash = hash_sequence(sequence, length);
    size_t index = hash % map->capacity;

    for (size_t i = 0; i < map->capacity; ++i) {
        size_t try = (index + i) % map->capacity;
        Entry* entry = &map->entries[try];

        if (!entry->used) return NULL;
        if (sequences_equal(entry->binary_sequence, entry->length, sequence, length)) {
            return entry;
        }
    }
    return NULL;
}

// Put without malloc
int binseq_map_put(BinSeqMap* map,
                   const uint8_t* key_sequence, uint16_t key_length,
                   int value_frequency) {
    if (!map || !key_sequence || key_length == 0) return 0;

    Entry* existing = find_entry(map, key_sequence, key_length);
    if (existing) {
        existing->frequency = value_frequency;
        return 1;
    }

    uint64_t hash = hash_sequence(key_sequence, key_length);
    size_t index = hash % map->capacity;

    for (size_t i = 0; i < map->capacity; ++i) {
        size_t try = (index + i) % map->capacity;
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

    return 0; // no space
}

// Get frequency pointer
const int* binseq_map_get_frequency(const BinSeqMap* map,
                                    const uint8_t* key_sequence, uint16_t key_length) {
    Entry* entry = find_entry(map, key_sequence, key_length);
    return entry ? &entry->frequency : NULL;
}

// Increment frequency if exists
int binseq_map_increment_frequency(BinSeqMap* map,
                                   const uint8_t* key_sequence, uint16_t key_length) {
    Entry* entry = find_entry(map, key_sequence, key_length);
    if (!entry) return 0;
    entry->frequency++;
    return 1;
}

// Reset the map for reuse (no freeing)
void binseq_map_reset(BinSeqMap* map) {
    if (!map) return;

    for (size_t i = 0; i < map->capacity; ++i) {
        Entry* entry = &map->entries[i];
        entry->binary_sequence = NULL;
        entry->length = 0;
        entry->frequency = 0;
        entry->used = 0;
    }

    map->size = 0;
}
