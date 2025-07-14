// bit_reader.c
#include "bit_reader.h"

void bitreader_init(BitReader* br, const uint8_t* buffer, size_t size) {
    br->buffer = buffer;
    br->buffer_size = size;
    br->byte_pos = 0;
    br->bit_pos = 0;
    br->overflow = false;
}

bool bitreader_read(BitReader* br, uint32_t* value, uint8_t num_bits) {
    if (num_bits > 32 || br->overflow) return false;
    *value = 0;

    for (int i = num_bits - 1; i >= 0; --i) {
        if (br->byte_pos >= br->buffer_size) {
            br->overflow = true;
            return false;
        }

        uint8_t current_byte = br->buffer[br->byte_pos];
        uint8_t bit = (current_byte >> (7 - br->bit_pos)) & 1;
        *value |= (bit << i);

        br->bit_pos++;
        if (br->bit_pos == 8) {
            br->bit_pos = 0;
            br->byte_pos++;
        }
    }
    return true;
}
