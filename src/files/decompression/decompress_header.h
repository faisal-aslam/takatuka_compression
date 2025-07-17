#pragma once

#include <stdint.h>
#include <stdio.h>
#include "../map/code_map.h"

// Reconstructs CodeMap by reading the header of the compressed file
void read_header_and_create_code_map(FILE* file_to_read);

