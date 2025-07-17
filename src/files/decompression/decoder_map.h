#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    const uint8_t* seq;
    uint8_t length;
    uint16_t code;
    uint8_t code_class;
    bool occupied;
} DecoderMapEntry;

typedef struct {
    DecoderMapEntry* entries;
    size_t capacity;
    size_t size;
} DecoderMap;

void init_decoder_map(DecoderMap* map, size_t capacity);
void free_decoder_map(DecoderMap* map);

bool decoder_map_set(DecoderMap* map, uint16_t code, uint8_t code_class, const uint8_t* seq, uint8_t length);
bool decoder_map_get(const DecoderMap* map, uint16_t code, uint8_t code_class, const uint8_t** out_seq, uint8_t* out_len);

void print_decoder_map(const DecoderMap* map);
