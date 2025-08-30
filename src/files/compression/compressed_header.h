#pragma once

#pragma once

#include "best_path_view.h"
#include "code_map.h"
#include "seq_freq_map.h"
#include "bit_writer.h"
#include <stdint.h>
#include <string.h>

//rle count length. It is computed dynamically.
extern uint8_t global_rle_bits;  

extern CodeMap code_map;
/*
 * Exposed symbol so compressed_body.c can use same class2 bit-width
 * as written into the header by populate_header().
 *
 * This value is written into the compressed file as a single byte
 * and should be read by any decoder prior to decoding body.
 *
 *  - 0 means "no class2 codes present"
 *  - otherwise value is number of bits used for class2 indices
 */
extern uint8_t global_class2_bits;

// Function to populate header
void populate_header(BestPathView best_path, const uint8_t *block, FILE *file_to_write, BitWriter *writer);

