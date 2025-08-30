//decompress.c

// decompress.c

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

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input> <output>\n", argv[0]);
        return 1;
    }

    printf("Decompressing %s to %s...\n", argv[1], argv[2]);
    read_compressed_file(argv[1], argv[2]);
    printf("Done Decompression.\n");
    return 0;
}


// In decompress.c, update the cleanup section:

void read_compressed_file(const char* input_file_name, const char* output_file_name) {
    if (!input_file_name || !output_file_name) {
        fprintf(stderr, "Error: Invalid file names\n");
        return;
    }

    FILE *file = fopen(input_file_name, "rb");
    if (!file) {
        perror("Failed to open input file");
        return;
    }

    BitReader reader;
    bitreader_attach_file(&reader, file, HEADER_BUFFER_SIZE);

    // Initialize global variables to clean state
    global_class2_bits = 0;
    global_rle_bits = 0;
    
    // Initialize decoder map to clean state
    free_decoder_map(&decoder_map); // Free if already allocated
    init_decoder_map(&decoder_map, 0); // Start with capacity 0
    
    // Free any existing codebook pool
    free_decoder_codebook_pool();

    read_header_and_create_decoder_map(&reader);
    printf("Read header\n");

    read_body_using_decoder_map(&reader, output_file_name);
    printf("Read body\n");

    // Cleanup in correct order
    bitreader_close(&reader);
    fclose(file);
    
    // Free decoder resources
    free_decoder_map(&decoder_map);
    free_decoder_codebook_pool();

    printf("Decompression completed successfully\n");
}