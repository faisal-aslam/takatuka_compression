// bit_reader.h
#ifndef BIT_READER_H
#define BIT_READER_H

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
} BitReader;

void bitreader_init(BitReader* br, const uint8_t* buffer, size_t size);
bool bitreader_read(BitReader* br, uint32_t* value, uint8_t num_bits);
uint8_t* bitreader_load_from_file(FILE* fp, size_t* out_size);

#endif // BIT_READER_H
