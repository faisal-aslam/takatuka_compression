// decompress_header.h

#pragma once

#include "bit_reader.h"
#include "decoder_map.h"

extern DecoderMap decoder_map;

// Reconstructs DecoderMap by reading the header of the compressed file
void read_header_and_create_decoder_map(BitReader* reader);
