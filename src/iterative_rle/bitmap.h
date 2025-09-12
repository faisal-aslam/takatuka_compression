#pragma once

#include <stdint.h>
#include <stddef.h>

size_t pack_bitmap(uint8_t *packed, const uint8_t *bits, size_t bit_count);
void unpack_bitmap(uint8_t *bits, size_t bit_count, const uint8_t *packed);
void print_bitmap(const uint8_t *bits, size_t bit_count);


