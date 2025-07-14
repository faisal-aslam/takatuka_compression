// bit_writer.c
#include "bit_writer.h"

void bitwriter_init(BitWriter* bw, uint8_t* buffer, size_t size) {
    bw->buffer = buffer;
    bw->buffer_size = size;
    bw->byte_pos = 0;
    bw->bit_pos = 0;
    bw->overflow = false;
}

bool bitwriter_write(BitWriter* bw, uint32_t value, uint8_t num_bits) {
    if (num_bits > 32 || bw->overflow) return false;

    for (int i = num_bits - 1; i >= 0; --i) {
        if (bw->byte_pos >= bw->buffer_size) {
            bw->overflow = true;
            return false;
        }

        uint8_t bit = (value >> i) & 1;
        bw->buffer[bw->byte_pos] |= bit << (7 - bw->bit_pos);
        bw->bit_pos++;

        if (bw->bit_pos == 8) {
            bw->bit_pos = 0;
            bw->byte_pos++;
        }
    }
    return true;
}

void bitwriter_flush(BitWriter* bw) {
    if (bw->bit_pos != 0) {
        bw->byte_pos++;
        bw->bit_pos = 0;
    }
}

size_t bitwriter_bytes_written(const BitWriter* bw) {
    return bw->byte_pos + (bw->bit_pos != 0 ? 1 : 0);
}

