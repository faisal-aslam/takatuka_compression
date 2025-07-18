// bit_writer.c
#include "bit_writer.h"
#include <string.h>

void bitwriter_init(BitWriter* bw, uint8_t* buffer, size_t size) {
    bw->buffer = buffer;
    bw->buffer_size = size;
    bw->byte_pos = 0;
    bw->bit_pos = 0;
    bw->overflow = false;
    memset(buffer, 0, size);
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

bool bitwriter_write_to_file(const BitWriter* bw, FILE* fp) {
    size_t bytes_to_write = bitwriter_bytes_written(bw);
    return fwrite(bw->buffer, 1, bytes_to_write, fp) == bytes_to_write;
}

void bitwriter_print_state(const BitWriter* bw) {
    printf("[BitWriter] byte_pos = %zu, bit_pos = %u, total_bits = %zu, overflow = %s\n",
           bw->byte_pos, bw->bit_pos,
           bw->byte_pos * 8 + bw->bit_pos,
           bw->overflow ? "true" : "false");
}


bool bitwriter_overwrite_at(BitWriter* bw, size_t bit_pos, uint32_t value, uint8_t num_bits) {
    if (num_bits > 32 || bw->overflow) return false;

    size_t max_bits = bw->buffer_size * 8;
    if (bit_pos + num_bits > max_bits) {
        bw->overflow = true;
        return false;
    }

    for (int i = num_bits - 1; i >= 0; --i) {
        size_t current_bit = bit_pos + (num_bits - 1 - i);
        size_t byte_index = current_bit / 8;
        size_t bit_index = 7 - (current_bit % 8);  // MSB-first

        uint8_t bit = (value >> i) & 1;
        if (bit)
            bw->buffer[byte_index] |= (1 << bit_index);
        else
            bw->buffer[byte_index] &= ~(1 << bit_index);
    }

    return true;
}


void bitwriter_reset(BitWriter* bw) {
    bw->byte_pos = 0;
    bw->bit_pos = 0;
    bw->overflow = false;
    memset(bw->buffer, 0, bw->buffer_size);
}


void bitwriter_reset_positions(BitWriter* bw) {
    bw->byte_pos = 0;
    bw->bit_pos = 0;
    bw->overflow = false;
}
