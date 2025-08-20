// bit_reader.h =====

#pragma once


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>



/*
* Robust BitReader with streaming support.
*
* - Supports attaching to a FILE* and reading in chunks (owned_buf).
* - Preserves partial-byte state across chunk refills: if a read stops in the
* middle of a byte, the remaining bits of that byte are preserved and used
* when the next chunk is loaded.
* - bitreader_read(...) will automatically attempt to refill from the file
* when the current in-memory buffer runs out.
* - bitreader_peek(...) does NOT trigger a refill: it only peeks within the
* currently loaded buffer (safe for quick lookahead without side effects).
*
* Usage notes:
* - Call bitreader_attach_file() to set up streaming from a file. The initial
* chunk is read immediately.
* - When bitreader_read() returns false, check br->overflow or errno if you
* need diagnostics. If it returns false in the middle of a large read it's
* usually EOF.
*/


typedef struct {
    /* Current readable window */
    const uint8_t* buffer;   /* points to owned_buf when attached to a file; otherwise external */
    size_t         buffer_size;  /* bytes currently valid in buffer */
    size_t         byte_pos;     /* current byte index in buffer */
    uint8_t        bit_pos;      /* current bit index in current byte: 0 (MSB)..7 (LSB) */

    /* Error/EOF state */
    bool           overflow;     /* sticky error: set on unrecoverable failure */

    /* Optional backing file + owned staging buffer */
    FILE*          file;         /* non-NULL if attached to a file */
    uint8_t*       owned_buf;    /* staging buffer we own when file-attached */
    size_t         buffer_cap;   /* capacity of owned_buf in bytes */
} BitReader;

/* Attach to an existing memory buffer (we do not take ownership) */
void bitreader_init(BitReader* br, const uint8_t* buffer, size_t size);

/* Attach to a FILE* and allocate an internal buffer of capacity buffer_cap, preloading the first chunk */
void bitreader_attach_file(BitReader* br, FILE* file, size_t buffer_cap);

/* Read num_bits (<=32) into *value; returns true on success, false on (true) EOF/error.
   This function auto-refills from FILE* as needed and is atomic (state restored on failure). */
bool bitreader_read(BitReader* br, uint32_t* value, uint8_t num_bits);

/* Non-destructive lookahead inside the currently loaded buffer only (no auto-refill). */
bool bitreader_peek(BitReader* br, uint32_t* value, uint8_t num_bits);

/* Force-aligned to next byte; if at end of buffer, the next read will refill automatically. */
void bitreader_move_byte_boundary(BitReader* br);

/* Replace the in-memory buffer (no ownership taken). Resets position and clears errors. */
void bitreader_reset(BitReader* br, const uint8_t* new_buffer, size_t new_size);

/* Utility: read entire FILE into a newly malloc’d buffer. Caller owns/free(). */
uint8_t* bitreader_load_from_file(FILE* fp, size_t* out_size);

/* Manual chunk refill (mainly for specialized uses/tests). Returns true if more data was loaded. */
bool bitreader_fill_next_chunk(BitReader* br);

/* Debug helper */
void bitreader_print_state(const BitReader* br);

/* Release internal resources (owned buffer). Safe to call regardless of how the reader was initialized. */
void bitreader_close(BitReader* br);
