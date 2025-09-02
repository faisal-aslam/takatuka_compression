#pragma once

#include <stdint.h>
extern char output_file[500];

long process_block(const uint8_t *block, uint32_t block_size);

