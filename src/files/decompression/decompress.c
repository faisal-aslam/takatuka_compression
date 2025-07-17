#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h> 
#include "decompress.h"
#include "code_classes.h"
#include "best_path_view.h"
#include "compressed_header.h"
#include "code_map.h"
#include "compressed_body.h"
#include "decompress_header.h"

void read_compressed_file(BestPathView best_view, const char* filename, const uint8_t* block) {


    if (!filename || !block) {
        fprintf(stderr, "Error: Invalid inputs in writeCompressedOutput\n");
        return;
    }

    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open binary reading file");
        return;
    }
    read_header_and_create_code_map(file);   

}