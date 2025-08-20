// bit_writer.h

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/*
 * BitWriter — robust, all-or-nothing bitstream writer.
 *
 * Public fields kept for compatibility with existing code:
 *   - buffer / buffer_size are accepted by init but not used for storage;
 *     the writer manages its own dynamic buffer internally.
 *   - byte_pos / bit_pos reflect the current end-of-stream cursor.
 *   - overflow is maintained for API compatibility; it is set only for
 *     invalid inputs (e.g., num_bits > 32), never for capacity growth.
 *
 * Key guarantees:
 *   - bitwriter_write(): either writes the entire value OR returns false
 *     without modifying the stream (no partial writes).
 *   - bitwriter_overwrite_at(): patches bits anywhere in the current
 *     in-memory stream (absolute bit positions from 0).
 *   - bitwriter_write_to_file(): writes all buffered bytes to FILE* and
 *     then clears/releases the internal buffer so there are no leaks.
 */

typedef struct {
    /* legacy / public fields (kept for external code that reads them) */
    uint8_t *buffer;    /* not used for storage; preserved for ABI */
    size_t buffer_size; /* not used for storage; preserved for ABI */
    size_t byte_pos;    /* current write byte position (end of stream) */
    uint8_t bit_pos;    /* next bit position within current byte [0..7] */
    bool overflow;      /* set only on invalid input */

    /* internal dynamic storage */
    uint8_t *_data; /* owned contiguous byte buffer */
    size_t _cap;    /* allocated capacity in bytes */
    size_t _size;   /* bytes currently used (== bytes written so far) */
} BitWriter;

#ifdef __cplusplus
extern "C" {
#endif

void bitwriter_init(BitWriter *bw, uint8_t *buffer, size_t size);
size_t bitwriter_bytes_written(const BitWriter *bw);
bool bitwriter_write_to_file(const BitWriter *bw, FILE *fp);
void bitwriter_print_state(const BitWriter *bw);
bool bitwriter_overwrite_at(BitWriter *bw, size_t bit_pos, uint32_t value, uint8_t num_bits);
void bitwriter_reset(BitWriter *bw);           /* clear stream + free internal storage */
void bitwriter_reset_positions(BitWriter *bw); /* clear stream, keep capacity for reuse */

bool bitwriter_write(BitWriter *bw, uint32_t value, uint8_t num_bits
#ifdef DEBUG
                     ,
                     const char *label
#endif
);

#ifdef __cplusplus
}
#endif
