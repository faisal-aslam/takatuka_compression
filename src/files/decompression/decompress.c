//decompress.c

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h> 
#include "decompress.h"
#include "code_classes.h"
#include "code_map.h"
#include "decompress_body.h"
#include "decompress_header.h"

#define HEADER_BUFFER_SIZE 4096

void read_compressed_file(const char* filename, const uint8_t* block) {
    if (!filename || !block) {
        fprintf(stderr, "Error: Invalid inputs in writeCompressedOutput\n");
        return;
    }

    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open binary reading file");
        return;
    }
    BitReader reader;
    bitreader_attach_file(&reader, file, HEADER_BUFFER_SIZE);

    read_header_and_create_decoder_map(&reader);   // shared reader + buffer
    read_body_using_decoder_map(&reader, "output.bin");          // reuses buffer + position

    bitreader_close(&reader);    
    fclose(file);
}
