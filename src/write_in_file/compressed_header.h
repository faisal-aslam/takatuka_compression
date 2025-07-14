#ifndef COMPRESSED_HEADER_H
#define COMPRESSED_HEADER_H

#include <stdint.h>
#include <string.h>
#include "best_path_view.h"

// Platform-independent packed struct

#pragma pack(push, 1)  // Ensure no padding
typedef struct __attribute__((packed)) {
    uint8_t code_class : 2;  // 2-bit code class
    uint8_t seq_length;  // 8-bit sequence length (supports up to 256 bytes)
    uint8_t* code;      // Flexible array for code + sequence
} Code;
#pragma pack(pop)

// Safety check
//this is throwing an assert why 
//static_assert(sizeof(EncodedData) == 1, "Struct has padding!");

#pragma pack(push, 1)  // No padding
typedef struct {
    uint16_t  number_of_codes;  // Number of codes in the header.
    Code* codes;  // a array of codes.
} CompressedHeader;
#pragma pack(pop)


void populate_header(BestPathView best_path);
#endif