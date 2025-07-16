#include "code_map.h"
#include <stdlib.h>
#include <string.h>
#include "xxhash.h"
#include "general_map.h"

void init_code_map(CodeMap* map, size_t capacity) {
    map->capacity = capacity * 2; // keep load factor ≤ 0.5
    map->size = 0;
    map->entries = calloc(map->capacity, sizeof(CodeMapEntry));
}

void free_code_map(CodeMap* map) {
    free(map->entries);
    map->entries = NULL;
    map->capacity = 0;
    map->size = 0;
}

static inline uint64_t hash_seq(const uint8_t* seq, uint8_t len) {
    return XXH3_64bits(seq, len);
}

static inline size_t probe_index(size_t hash, size_t i, size_t cap) {
    return (hash + i) % cap;
}

bool code_map_set(CodeMap* map, const uint8_t* seq, uint8_t len, uint16_t code, uint8_t code_class) {
    uint64_t h = hash_seq(seq, len);
    for (size_t i = 0; i < map->capacity; ++i) {
        size_t idx = probe_index(h, i, map->capacity);
        CodeMapEntry* e = &map->entries[idx];

        if (!e->occupied) {
            e->seq = seq;
            e->length = len;
            e->code = code;
            e->code_class = code_class;
            e->hash = h;
            e->occupied = true;
            map->size++;
            return true;
        }

        if (e->occupied && e->length == len && e->hash == h && sequences_equal(e->seq, seq, len) == 0) {
            e->code = code;
            e->code_class = code_class;
            return true; // update
        }
    }
    return false; // map full
}

bool code_map_get(const CodeMap* map, const uint8_t* seq, uint8_t len, uint16_t* out_code, uint8_t* out_class) {
    uint64_t h = hash_seq(seq, len);
    for (size_t i = 0; i < map->capacity; ++i) {
        size_t idx = probe_index(h, i, map->capacity);
        CodeMapEntry* e = &map->entries[idx];

        if (!e->occupied) return false;
        if (e->length == len && e->hash == h && sequences_equal(e->seq, seq, len) == 0) {
            *out_code = e->code;
            *out_class = e->code_class;
            return true;
        }
    }
    return false;
}
