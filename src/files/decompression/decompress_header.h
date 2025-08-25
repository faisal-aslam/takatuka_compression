// decompress_header.h

#pragma once

#include "bit_reader.h"
#include "decoder_map.h"
#include <stdint.h>

/* decoder map reconstructed from header */
extern DecoderMap decoder_map;

/*
 * Number of bits used for class-2 indices in this file.
 * This is read from the file header (1 byte) and used by the body reader.
 *  - 0 means "no class2 codes present" (or class2_count==1 case where encoder used 0 bits).
 */
extern uint8_t global_class2_bits;

/* Reconstructs DecoderMap by reading the header of the compressed file */
void read_header_and_create_decoder_map(BitReader* reader);

/* Frees the single contiguous codebook pool allocated by the header reader. */
void free_decoder_codebook_pool(void);
