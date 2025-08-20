// bit_reader.h =====

#pragma once


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>


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
const uint8_t* buffer; /* pointer to current buffer (owned_buf or external) */
size_t buffer_size; /* number of valid bytes in buffer */
size_t byte_pos; /* current byte index within buffer */
uint8_t bit_pos; /* bit index within current byte [0..7] */
bool overflow; /* set on unrecoverable errors (invalid args / EOF without file) */


FILE* file; /* optional file for streaming refill */
uint8_t* owned_buf; /* internal buffer used when streaming from file */
size_t buffer_cap; /* capacity of owned_buf */
} BitReader;


void bitreader_attach_file(BitReader* br, FILE* file, size_t buffer_cap);
bool bitreader_fill_next_chunk(BitReader* br);


void bitreader_init(BitReader* br, const uint8_t* buffer, size_t size);
bool bitreader_read(BitReader* br, uint32_t* value, uint8_t num_bits);
uint8_t* bitreader_load_from_file(FILE* fp, size_t* out_size);


void bitreader_print_state(const BitReader* br);
void bitreader_reset(BitReader* br, const uint8_t* new_buffer, size_t new_size);


bool bitreader_peek(BitReader* br, uint32_t* value, uint8_t num_bits);


void bitreader_close(BitReader* br);


void bitreader_move_byte_boundary(BitReader *br);