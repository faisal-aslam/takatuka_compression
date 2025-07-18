#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct {
    uint8_t* buffer;
    size_t buffer_size;
    size_t byte_pos;
    uint8_t bit_pos;  // 0 (MSB) to 7 (LSB)
    bool overflow;
} BitWriter;

void bitwriter_init(BitWriter* bw, uint8_t* buffer, size_t size);
bool bitwriter_write(BitWriter* bw, uint32_t value, uint8_t num_bits);
void bitwriter_flush(BitWriter* bw);
size_t bitwriter_bytes_written(const BitWriter* bw);
bool bitwriter_write_to_file(const BitWriter* bw, FILE* fp);
void bitwriter_print_state(const BitWriter* bw);
bool bitwriter_overwrite_at(BitWriter* bw, size_t bit_pos, uint32_t value, uint8_t num_bits);
void bitwriter_reset(BitWriter* bw);
void bitwriter_reset_positions(BitWriter* bw);

