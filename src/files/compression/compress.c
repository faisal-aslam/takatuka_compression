//compress.c main file to write compressed data

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h> 
#include "compress.h"
#include "code_classes.h"
#include "best_path_view.h"
#include "compressed_header.h"
#include "code_map.h"
#include "compressed_body.h"
#include "bit_writer.h"

#define BUFFER_SIZE 4096


/**
  * @brief Main function to write complete compressed output file
  * 
  * @param filename Output file path
  * @param sequences Array of binary sequences for header
  * @param seq_count Number of sequences
  * @param best_node TreeNode with best compression path
  * @param block Pointer to raw data block
  */
long write_compressed_output(const char* filename, const uint8_t* block) {
    if (!filename || !block) {
        fprintf(stderr, "Error: Invalid inputs in writeCompressedOutput\n");
        return 0;
    }

    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open output file");
        return 0;
    }

 #ifdef DEBUG
    printf("[DEBUG] Initial best path view:\n");
    print_best_view(&view, 1, block);
#endif

    printf("\n ==== Starting compressed output writing === \n");

    // Allocate a single buffer for both header and body
    size_t buffer_size = BUFFER_SIZE;
    uint8_t* buffer = malloc(buffer_size);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate buffer\n");
        fclose(file);
        return 0;
    }

    BitWriter writer;
    bitwriter_init(&writer, buffer, buffer_size);

#ifdef DEBUG
    printf("[DEBUG] Initialized BitWriter with buffer size: %zu\n", buffer_size);
#endif

    // Write header
    long header_size = populate_header(view, block, file, &writer);

#ifdef DEBUG
    printf("[DEBUG] After header writing:\n");
    print_best_view(&view, 1, block);
    printf("[DEBUG] BitWriter state after header:\n");
    bitwriter_print_state(&writer);
#endif

    // Write body
    long body_size = populate_body(view, block, file, &writer);

#ifdef DEBUG
    printf("[DEBUG] BitWriter state after body:\n");
    bitwriter_print_state(&writer);
#endif

    printf("\n === Writing compressed output completed ==\n");
   
    free(buffer);
    if (fclose(file) != 0) {
        perror("Warning: Error closing output file");
    }
    return body_size+header_size;
}