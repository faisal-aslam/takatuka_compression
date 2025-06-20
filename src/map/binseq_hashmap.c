//binseq_hashmap.c
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
static Entry* find_entry(const BinSeqMap* map,
                         const uint8_t* sequence, uint16_t length) {
    if (!map || map->capacity == 0 || !sequence || length == 0)
        return NULL;  // Defensive guard

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


int binseq_map_resize(BinSeqMap* map, size_t min_new_capacity) {
    if (!map) return 0;

    size_t new_capacity = map->capacity == 0 ? INITIAL_CAPACITY : map->capacity;
    
    // Exponential growth until we reach min_new_capacity
    while (new_capacity < min_new_capacity) {
        new_capacity *= GROWTH_FACTOR;
    }

    Entry* new_entries = calloc(new_capacity, sizeof(Entry));
    if (!new_entries) return 0;

    // Rehash existing entries if any
    if (map->entries) {
        for (size_t i = 0; i < map->capacity; ++i) {
            Entry* old_entry = &map->entries[i];
            if (old_entry->used) {
                uint64_t hash = XXH3_64bits(old_entry->binary_sequence, old_entry->length);
                size_t new_index = hash % new_capacity;

                // Find empty slot in new table
                while (new_entries[new_index].used) {
                    new_index = (new_index + 1) % new_capacity;
                }

                new_entries[new_index] = *old_entry;
            }
        }
        free(map->entries);
    }

    map->entries = new_entries;
    map->capacity = new_capacity;
    return 1;
}

int binseq_map_increment_frequency(BinSeqMap* map,
                                 const uint8_t* key_sequence, uint16_t key_length) {
    // Fast path checks
    if (!map || !key_sequence || key_length == 0) return 0;

    // Handle empty map case
    if (map->capacity == 0) {
        if (!binseq_map_resize(map, INITIAL_CAPACITY)) return 0;
        // Don't recurse - just continue to insertion attempt
    }

    const uint64_t hash = XXH3_64bits(key_sequence, key_length);
    size_t index = hash % map->capacity;
    Entry* entries = map->entries;

    // Single probing loop
    for (size_t i = 0; i < map->capacity; ++i) {
        Entry* entry = &entries[index];
        
        if (entry->used) {
            if (entry->length == key_length && 
                memcmp(entry->binary_sequence, key_sequence, key_length) == 0) {
                entry->frequency++;
                return 1;
            }
        } else {
            // Found empty slot - insert
            entry->binary_sequence = (uint8_t*)key_sequence;
            entry->length = key_length;
            entry->frequency = 1;
            entry->used = 1;
            map->size++;
            return 1;
        }
        
        index = (index + 1) % map->capacity;
    }

    // Map is full - resize and try again (but only once)
    if (!binseq_map_resize(map, map->capacity * GROWTH_FACTOR)) return 0;
    
    // Non-recursive retry
    uint64_t new_hash = XXH3_64bits(key_sequence, key_length);
    size_t new_index = new_hash % map->capacity;
    for (size_t i = 0; i < map->capacity; ++i) {
        Entry* entry = &map->entries[new_index];
        if (!entry->used) {
            entry->binary_sequence = (uint8_t*)key_sequence;
            entry->length = key_length;
            entry->frequency = 1;
            entry->used = 1;
            map->size++;
            return 1;
        }
        new_index = (new_index + 1) % map->capacity;
    }

    return 0; // Still no space after resize
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


// Fast lookup: returns matching Entry* or NULL (no update to size)
Entry* binseq_map_fast_lookup(Entry* entries, size_t capacity,
                            const uint8_t* key, uint16_t key_length) {
    if (!entries || capacity == 0 || !key || key_length == 0) 
        return NULL;

    // Fast path for single-byte sequences
    if (key_length == 1) {
        size_t index = key[0] % capacity;
        Entry* e = &entries[index];
        if (e->used && e->length == 1 && e->binary_sequence[0] == key[0])
            return e;
        return NULL;
    }

    uint64_t hash = XXH3_64bits(key, key_length);
    size_t index = hash % capacity;

    // Unroll first few probes
    for (int i = 0; i < 3; i++) {  // Check first 3 slots
        Entry* e = &entries[index];
        if (!e->used) return NULL;
        if (e->length == key_length && 
            memcmp(e->binary_sequence, key, key_length) == 0) {
            return e;
        }
        index = (index + 1) % capacity;
    }

    // Fall back to regular probing
    for (size_t i = 3; i < capacity; ++i) {
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


// Fast insert: inserts if empty slot found, assumes key is caller-managed
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
            e->binary_sequence = (uint8_t*)key; // assumed managed externally
            e->length = key_length;
            e->frequency = frequency;
            e->used = 1;
            return e;
        }
    }

    return NULL;
}


void print_hashmap(BinSeqMap *map) {
    if (!map || map->capacity == 0) {
        printf("Hashmap is NULL or has zero capacity.\n");
        return;
    }

    printf("Hashmap contents (size = %zu, capacity = %zu):\n", map->size, map->capacity);

    for (size_t i = 0; i < map->capacity; ++i) {
        Entry* e = &map->entries[i];

        if (!e->used)
            continue;

        printf("  [%zu] Length: %u, Frequency: %d, Sequence: ", i, e->length, e->frequency);
        for (uint16_t j = 0; j < e->length; ++j) {
            printf("%02X ", e->binary_sequence[j]);  // print in hex
        }
        printf("\n");
    }

    if (map->size == 0) {
        printf("  (empty)\n");
    }
}
