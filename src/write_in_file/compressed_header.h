#ifndef COMPRESSED_HEADER_H
#define COMPRESSED_HEADER_H

#pragma once

#include <stdint.h>
#include <string.h>
#include "best_path_view.h"

#pragma pack(push, 1)
typedef struct {
    uint8_t code_class : 2;
    uint8_t seq_length;
    uint8_t data[];  // code bits + sequence
} Code;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint16_t number_of_codes;
    Code** codes;  // Array of pointers to individually allocated Code blocks
} CompressedHeader;
#pragma pack(pop)

// Function to populate header
void populate_header(BestPathView best_path, const uint8_t* block);
void free_compressed_header(CompressedHeader* header);  // Free function

#endif