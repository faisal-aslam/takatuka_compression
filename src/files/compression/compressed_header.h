#pragma once

#include "best_path_view.h"
#include "code_map.h"
#include "seq_freq_map.h"
#include "bit_writer.h"
#include <stdint.h>
#include <string.h>

extern CodeMap code_map;
// Function to populate header
void populate_header(BestPathView best_path, const uint8_t *block, FILE *file_to_write, BitWriter *writer);
