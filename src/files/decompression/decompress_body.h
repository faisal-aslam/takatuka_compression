// decompress_header.h

#pragma once

#include "bit_reader.h"

// Read the body using the decoder map.
void read_body_using_decoder_map(BitReader* reader, const char* decompress_file_name);
