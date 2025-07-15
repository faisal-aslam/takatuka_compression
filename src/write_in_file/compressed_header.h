#ifndef COMPRESSED_HEADER_H
#define COMPRESSED_HEADER_H

#pragma once

#include <stdint.h>
#include <string.h>
#include "best_path_view.h"


// Function to populate header
void populate_header(BestPathView best_path, const uint8_t* block, FILE* file_to_write);

#endif