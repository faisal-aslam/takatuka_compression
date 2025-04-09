#include "binseq_hashmap.h"
#include <stdlib.h>
#include <string.h>
#include "xxhash.h"

typedef struct {
    BinSeqKey key;
    BinSeqValue value;
    int used;
} Entry;

struct BinSeqMap {
    Entry *entries;
    size_t capacity;
};

static inline uint64_t binseq_hash(BinSeqKey key) {
    return XXH3_64bits(key.binary_sequence, key.length);
}

static inline int binseq_equal(BinSeqKey a, BinSeqKey b) {
    return a.length == b.length && memcmp(a.binary_sequence, b.binary_sequence, a.length) == 0;
}

BinSeqMap *binseq_map_create(size_t capacity) {
    BinSeqMap *map = malloc(sizeof(BinSeqMap));
    map->entries = calloc(capacity, sizeof(Entry));
    map->capacity = capacity;
    return map;
}

void binseq_map_free(BinSeqMap *map) {
    if (!map) return;
    free(map->entries);
    free(map);
}

int binseq_map_put(BinSeqMap *map, BinSeqKey key, BinSeqValue value) {
    uint64_t hash = binseq_hash(key);
    size_t index = hash % map->capacity;

    for (size_t i = 0; i < map->capacity; i++) {
        size_t try = (index + i) % map->capacity;
        Entry *entry = &map->entries[try];

        if (!entry->used || binseq_equal(entry->key, key)) {
            entry->key = key;
            entry->value = value;
            entry->used = 1;
            return 1;
        }
    }
    return 0; // Map full
}

BinSeqValue *binseq_map_get(BinSeqMap *map, BinSeqKey key) {
    uint64_t hash = binseq_hash(key);
    size_t index = hash % map->capacity;

    for (size_t i = 0; i < map->capacity; i++) {
        size_t try = (index + i) % map->capacity;
        Entry *entry = &map->entries[try];

        if (entry->used && binseq_equal(entry->key, key)) {
            return &entry->value;
        }
    }
    return NULL;
}

int binseq_map_contains(BinSeqMap *map, BinSeqKey key) {
    return binseq_map_get(map, key) != NULL;
}

