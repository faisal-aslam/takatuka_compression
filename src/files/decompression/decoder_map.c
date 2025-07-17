#include "decoder_map.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void init_decoder_map(DecoderMap* map, size_t capacity) {
    map->capacity = capacity * 2; // Maintain load factor ≤ 0.5
    map->size = 0;
    map->entries = calloc(map->capacity, sizeof(DecoderMapEntry));
}

void free_decoder_map(DecoderMap* map) {
    free(map->entries);
    map->entries = NULL;
    map->capacity = 0;
    map->size = 0;
}

static inline size_t decoder_hash(uint16_t code, uint8_t code_class) {
    return ((uint32_t)code << 3) | (code_class & 0x07); // simple mix
}

static inline size_t decoder_probe(size_t hash, size_t i, size_t cap) {
    return (hash + i) % cap;
}

bool decoder_map_set(DecoderMap* map, uint16_t code, uint8_t code_class, const uint8_t* seq, uint8_t length) {
    if (map->size >= map->capacity / 2) {
        return false; // load factor too high
    }

    size_t h = decoder_hash(code, code_class);
    for (size_t i = 0; i < map->capacity; ++i) {
        size_t idx = decoder_probe(h, i, map->capacity);
        DecoderMapEntry* e = &map->entries[idx];

        if (!e->occupied) {
            e->code = code;
            e->code_class = code_class;
            e->seq = seq;
            e->length = length;
            e->occupied = true;
            map->size++;
            return true;
        }

        if (e->code == code && e->code_class == code_class) {
            e->seq = seq;
            e->length = length;
            return true; // update existing
        }
    }
    return false;
}

bool decoder_map_get(const DecoderMap* map, uint16_t code, uint8_t code_class, const uint8_t** out_seq, uint8_t* out_len) {
    size_t h = decoder_hash(code, code_class);
    for (size_t i = 0; i < map->capacity; ++i) {
        size_t idx = decoder_probe(h, i, map->capacity);
        const DecoderMapEntry* e = &map->entries[idx];

        if (!e->occupied) return false;
        if (e->code == code && e->code_class == code_class) {
            *out_seq = e->seq;
            *out_len = e->length;
            return true;
        }
    }
    return false;
}

void print_decoder_map(const DecoderMap* map) {
    printf("DecoderMap (size: %zu, capacity: %zu):\n", map->size, map->capacity);
    printf("---------------------------------------------------\n");
    printf("| Index | Code | Class | Length | Sequence\n");
    printf("---------------------------------------------------\n");

    for (size_t i = 0; i < map->capacity; ++i) {
        const DecoderMapEntry* e = &map->entries[i];
        if (!e->occupied) continue;

        printf("| %5zu | %4u | %5u | %6u | ", i, e->code, e->code_class, e->length);
        for (uint8_t j = 0; j < e->length; ++j) {
            printf("%02X ", e->seq[j]);
        }
        printf("\n");
    }
    printf("---------------------------------------------------\n");
}
