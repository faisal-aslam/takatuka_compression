//iterative_rle/fileio.c

#include "fileio.h"
#include <stdio.h>
#include <stdlib.h>

/* Read entire binary file */
int read_input_file(const char *filename, uint8_t **buffer, size_t *size) {
    FILE *f = fopen(filename, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    rewind(f);
    *buffer = malloc(*size);
    if (!*buffer) { fclose(f); return 0; }
    fread(*buffer, 1, *size, f);
    fclose(f);
    return 1;
}

/* Write binary file with header + bitmap + data */
int write_output_file(const char *filename,
                      const uint8_t *data, size_t size,
                      const uint8_t *packed_bitmap, size_t packed_size,
                      uint8_t max_block_size) {
    FILE *f = fopen(filename, "wb");
    if (!f) return 0;

    /* header: bitmap size (1 byte) + max_block_size (1 byte) */
    fputc((uint8_t)packed_size, f);
    fputc(max_block_size, f);

    /* bitmap */
    fwrite(packed_bitmap, 1, packed_size, f);

    /* data */
    fwrite(data, 1, size, f);

    fclose(f);
    return 1;
}
