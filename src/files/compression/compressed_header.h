#pragma once

#pragma once

#include <stdint.h>
#include <string.h>
#include "best_path_view.h"
#include "seq_freq_map.h"
#include "code_map.h"

extern CodeMap code_map;
// Function to populate header
void populate_header(BestPathView best_path, const uint8_t* block, FILE* file_to_write);

