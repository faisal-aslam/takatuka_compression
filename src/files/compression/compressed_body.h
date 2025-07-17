#pragma once

#include <stdint.h>
#include <stdio.h>  
#include <string.h>
#include "best_path_view.h"

/**
 * @brief Processes BestPathView and writes compressed data to output file
 * 
 * Full processing logic:
 * 1. Reverse traversal of BestPathView
 * 2. Handles uncompressed nodes (prefix 0 + raw byte for each byte)
 * 3. Handles compressed non-RLE nodes (prefix 1 + code class + code)
 * 4. Handles RLE nodes (prefix 1 + repeat len + RLE count + sequence)
 * 
 * @param best_path The optimal compression path
 * @param block Input data block
 * @param file_to_write Output file handle
 */
void populate_body(BestPathView best_path, const uint8_t* block, FILE* file_to_write);

