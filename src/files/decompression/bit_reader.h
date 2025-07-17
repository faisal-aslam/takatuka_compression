#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct {
    const uint8_t* buffer;
    size_t buffer_size;
    size_t byte_pos;
    uint8_t bit_pos;  // 0 (MSB) to 7 (LSB)
    bool overflow;

    FILE* file;           // Optional: file to refill from
    uint8_t* owned_buf;   // Internal buffer owned by reader
    size_t   buffer_cap;  // Capacity of owned_buf
} BitReader;

void bitreader_attach_file(BitReader* br, FILE* file, size_t buffer_cap);
bool bitreader_fill_next_chunk(BitReader* br);

void bitreader_init(BitReader* br, const uint8_t* buffer, size_t size);
bool bitreader_read(BitReader* br, uint32_t* value, uint8_t num_bits);
uint8_t* bitreader_load_from_file(FILE* fp, size_t* out_size);

void bitreader_print_state(const BitReader* br); 
void bitreader_reset(BitReader* br, const uint8_t* new_buffer, size_t new_size);
