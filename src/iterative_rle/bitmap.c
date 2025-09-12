#include "bitmap.h"
#include <stdio.h>

/* Pack bits into bytes (MSB-first) */
size_t pack_bitmap(uint8_t *packed, const uint8_t *bits, size_t bit_count) {
    size_t byte_count = (bit_count + 7) / 8;
    for (size_t i = 0; i < byte_count; i++) packed[i] = 0;

    for (size_t i = 0; i < bit_count; i++) {
        size_t byte_index = i / 8;
        size_t bit_index = 7 - (i % 8);
        if (bits[i]) {
            packed[byte_index] |= (1 << bit_index);
        }
    }
    return byte_count;
}

/* Unpack bytes into bits */
void unpack_bitmap(uint8_t *bits, size_t bit_count, const uint8_t *packed) {
    for (size_t i = 0; i < bit_count; i++) {
        size_t byte_index = i / 8;
        size_t bit_index = 7 - (i % 8);
        bits[i] = (packed[byte_index] >> bit_index) & 1;
    }
}

void print_bitmap(const uint8_t *bits, size_t bit_count) {
    for (size_t i = 0; i < bit_count; i++) {
        printf("%u", bits[i]);
        if ((i + 1) % 8 == 0) printf(" ");
    }
    printf("\n");
}
