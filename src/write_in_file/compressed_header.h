//compressed_header.h

#ifndef COMPRESSED_HEADER_H
#define COMPRESSED_HEADER_H

#pragma once

#include <stdint.h>
#include <string.h>
#include "best_path_view.h"
#include "../map/seq_freq_map.h"
#include "../map/code_map.h"

extern CodeMap code_map;
// Function to populate header
void populate_header(BestPathView best_path, const uint8_t* block, FILE* file_to_write);

#endif