// iterative_rle/fileio.c

// fileio.c
#include "fileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

int read_input_file(const char *filename, uint8_t **buffer, size_t *size) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("fopen");
        return 0;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(f);
        return 0;
    }
    long ft = ftell(f);
    if (ft < 0) {
        perror("ftell");
        fclose(f);
        return 0;
    }
    *size = (size_t)ft;
    rewind(f);

    if (*size == 0) {
        *buffer = NULL;
        fclose(f);
        return 1;
    }

    *buffer = malloc(*size);
    if (!*buffer) {
        perror("malloc");
        fclose(f);
        return 0;
    }

    size_t nread = fread(*buffer, 1, *size, f);
    if (nread != *size) {
        fprintf(stderr, "Error: expected %zu bytes, got %zu (%s)\n", *size, nread, strerror(errno));
        fclose(f);
        free(*buffer);
        return 0;
    }

    fclose(f);
    return 1;
}

/* Write: [2 bytes bitmap_len (big-endian)] [1 byte max_block] [bitmap bytes] [data bytes] */
int write_output_file(const char *filename,
                      const uint8_t *data, size_t size,
                      const uint8_t *packed_bitmap, size_t packed_size,
                      uint8_t max_block_size) {
    FILE *f = fopen(filename, "wb");
    if (!f) {
        perror("fopen");
        return 0;
    }

    if (packed_size > 0xFFFF) {
        fprintf(stderr, "Error: packed bitmap too large (%zu bytes). Increase header field size.\n", packed_size);
        fclose(f);
        return 0;
    }

    /* Write 2-byte bitmap length (big-endian) */
    uint16_t ps = (uint16_t)packed_size;
    uint8_t hi = (uint8_t)((ps >> 8) & 0xFF);
    uint8_t lo = (uint8_t)(ps & 0xFF);

    if (fwrite(&hi, 1, 1, f) != 1) { perror("fwrite"); fclose(f); return 0; }
    if (fwrite(&lo, 1, 1, f) != 1) { perror("fwrite"); fclose(f); return 0; }

    /* Write max_block_size (1 byte) */
    if (fwrite(&max_block_size, 1, 1, f) != 1) { perror("fwrite"); fclose(f); return 0; }

    /* Write packed bitmap bytes (if any) */
    if (packed_size > 0) {
        if (fwrite(packed_bitmap, 1, packed_size, f) != packed_size) {
            perror("fwrite");
            fclose(f);
            return 0;
        }
    }

    /* Write data */
    if (size > 0) {
        if (fwrite(data, 1, size, f) != size) {
            perror("fwrite");
            fclose(f);
            return 0;
        }
    }

    fclose(f);
    return 1;
}
