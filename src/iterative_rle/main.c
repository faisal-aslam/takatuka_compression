#include "sort.h"
#include "bitmap.h"
#include "fileio.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s input.bin output.bin [max_block_size]\n", argv[0]);
        return 1;
    }

    uint8_t *data = NULL;
    size_t size = 0;
    if (!read_input_file(argv[1], &data, &size)) {
        fprintf(stderr, "Failed to read input file %s\n", argv[1]);
        return 1;
    }

    int max_block_size = (argc > 3) ? atoi(argv[3]) : (int)size;
    sort_info.original_size = size;
    sort_info.bitmap_size = 0;

    merge_sort(data, (int)size, max_block_size);

    /* Pack bitmap */
    uint8_t *packed = malloc((sort_info.bitmap_size + 7) / 8);
    size_t packed_size = pack_bitmap(packed, sort_info.bitmap, sort_info.bitmap_size);

    if (!write_output_file(argv[2], data, size, packed, packed_size, (uint8_t)max_block_size)) {
        fprintf(stderr, "Failed to write output file %s\n", argv[2]);
        free(packed);
        free(data);
        return 1;
    }

    printf("Wrote %s with %zu bytes of data and %zu bytes of bitmap\n",
           argv[2], size, packed_size);

    free(packed);
    free(data);
    return 0;
}
