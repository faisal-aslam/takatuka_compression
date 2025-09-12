//fileio.h

#pragma once
#include <stdint.h>
#include <stddef.h>

int read_input_file(const char *filename, uint8_t **buffer, size_t *size);
int write_output_file(const char *filename,
                      const uint8_t *data, size_t size,
                      const uint8_t *packed_bitmap, size_t packed_size,
                      uint8_t max_block_size);

