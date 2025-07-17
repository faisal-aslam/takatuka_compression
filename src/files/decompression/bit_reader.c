// bit_reader.c
#include "bit_reader.h"
#include <stdlib.h>

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

uint8_t* bitreader_load_from_file(FILE* fp, size_t* out_size) {
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    rewind(fp);

    uint8_t* buffer = malloc(size);
    if (!buffer) return NULL;

    if (fread(buffer, 1, size, fp) != size) {
        free(buffer);
        return NULL;
    }

    if (out_size) *out_size = size;
    return buffer;
}

void bitreader_print_state(const BitReader* br) {
    printf("[BitReader] byte_pos = %zu, bit_pos = %u, total_bits = %zu, overflow = %s\n",
           br->byte_pos, br->bit_pos,
           br->byte_pos * 8 + br->bit_pos,
           br->overflow ? "true" : "false");
}


void bitreader_reset(BitReader* br, const uint8_t* new_buffer, size_t new_size) {
    br->buffer = new_buffer;
    br->buffer_size = new_size;
    br->byte_pos = 0;
    br->bit_pos = 0;
    br->overflow = false;
}


void bitreader_attach_file(BitReader* br, FILE* file, size_t buffer_cap) {
    br->owned_buf = malloc(buffer_cap);
    if (!br->owned_buf) {
        fprintf(stderr, "Failed to allocate internal bitreader buffer\n");
        exit(EXIT_FAILURE);
    }

    br->file = file;
    br->buffer_cap = buffer_cap;

    size_t bytes_read = fread(br->owned_buf, 1, buffer_cap, br->file);
    br->buffer = br->owned_buf;
    br->buffer_size = bytes_read;
    br->byte_pos = 0;
    br->bit_pos = 0;
    br->overflow = false;
}

bool bitreader_fill_next_chunk(BitReader* br) {
    if (!br->file || !br->owned_buf) return false;

    size_t bytes_read = fread(br->owned_buf, 1, br->buffer_cap, br->file);
    if (bytes_read == 0) {
        br->overflow = true;
        return false;
    }

    br->buffer = br->owned_buf;
    br->buffer_size = bytes_read;
    br->byte_pos = 0;
    br->bit_pos = 0;
    br->overflow = false;

    return true;
}
