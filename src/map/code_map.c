//code_map.c

#include "code_map.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>  // For printf
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

static inline size_t probe_index(uint64_t hash, size_t i, size_t cap) {
    return (hash + i) % cap;
}

bool code_map_set(CodeMap* map, const uint8_t* seq, uint8_t len, uint16_t code, uint8_t code_class) {
    if (map->size >= map->capacity / 2) {
        return false; // Map is too full (load factor > 0.5)
    }

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

        if (e->hash == h && e->length == len && sequences_equal(e->seq, seq, len)) {
            e->code = code;
            e->code_class = code_class;
            return true; // update existing entry
        }
    }
    return false; // shouldn't reach here if load factor is maintained
}

bool code_map_get(const CodeMap* map, const uint8_t* seq, uint8_t len, uint16_t* out_code, uint8_t* out_class) {
    uint64_t h = hash_seq(seq, len);
    for (size_t i = 0; i < map->capacity; ++i) {
        size_t idx = probe_index(h, i, map->capacity);
        const CodeMapEntry* e = &map->entries[idx];

        if (!e->occupied) return false;
        if (e->hash == h && e->length == len && sequences_equal(e->seq, seq, len)) {
            *out_code = e->code;
            *out_class = e->code_class;
            return true;
        }
    }
    return false;
}

void print_code_map(const CodeMap* map) {
    printf("CodeMap (size: %zu, capacity: %zu):\n", map->size, map->capacity);
    printf("-------------------------------------------------\n");
    printf("| Index | Occupied | Length | Code | Class | Hash (low 32) | Sequence\n");
    printf("-------------------------------------------------\n");
    
    for (size_t i = 0; i < map->capacity; ++i) {
        const CodeMapEntry* e = &map->entries[i];
        printf("| %5zu | %8s | %6u | %4u | %5u | %11llX | ", 
               i, 
               e->occupied ? "true" : "false",
               e->occupied ? e->length : 0,
               e->occupied ? e->code : 0,
               e->occupied ? e->code_class : 0,
               e->occupied ? (unsigned long long)(e->hash & 0xFFFFFFFF) : 0);
        
        if (e->occupied) {
            for (uint8_t j = 0; j < e->length; ++j) {
                printf("%02X ", e->seq[j]);
            }
        } else {
            printf("(empty)");
        }
        printf("\n");
    }
    printf("-------------------------------------------------\n");
}