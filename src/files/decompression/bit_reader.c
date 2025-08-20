// bit_reader.c

#include "bit_reader.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Initialize reader with an existing buffer (non-streaming) */
void bitreader_init(BitReader *br, const uint8_t *buffer, size_t size) {
    if (!br) return;
    br->buffer = buffer;
    br->buffer_size = size;
    br->byte_pos = 0;
    br->bit_pos = 0;
    br->overflow = false;

    br->file = NULL;
    br->owned_buf = NULL;
    br->buffer_cap = 0;
}

/* Attach reader to FILE* for streaming reads. It allocates owned_buf and
 * fills the initial chunk immediately. */
void bitreader_attach_file(BitReader *br, FILE *file, size_t buffer_cap) {
    if (!br || !file || buffer_cap == 0) {
        fprintf(stderr, "bitreader_attach_file: invalid args\n");
        exit(EXIT_FAILURE);
    }

    /* free previous owned_buf if present */
    if (br->owned_buf) {
        free(br->owned_buf);
        br->owned_buf = NULL;
    }

    br->owned_buf = (uint8_t*)malloc(buffer_cap);
    if (!br->owned_buf) {
        fprintf(stderr, "bitreader_attach_file: allocation failed\n");
        exit(EXIT_FAILURE);
    }

    br->file = file;
    br->buffer_cap = buffer_cap;

    size_t bytes_read = fread(br->owned_buf, 1, buffer_cap, br->file);
    br->buffer = br->owned_buf;
    br->buffer_size = bytes_read;
    br->byte_pos = 0;
    br->bit_pos = 0;
    br->overflow = false;
}

/* Internal refill helper. Preserves unread bytes (including the partially
 * consumed current byte) across the refill. If there's no more data to read
 * and nothing unread, returns false (EOF). */
bool bitreader_fill_next_chunk(BitReader *br) {
    if (!br || !br->owned_buf || !br->file) return false;

    /* bytes still unread in current buffer */
    size_t unread = 0;
    if (br->buffer_size > br->byte_pos) {
        unread = br->buffer_size - br->byte_pos;
    }

    /* If unread bytes occupy the whole capacity, grow the buffer to hold more. */
    if (unread >= br->buffer_cap) {
        size_t new_cap = br->buffer_cap * 2u;
        if (new_cap <= br->buffer_cap) new_cap = br->buffer_cap + 4096u; /* overflow guard */
        uint8_t *new_buf = (uint8_t*)realloc(br->owned_buf, new_cap);
        if (!new_buf) return false;
        br->owned_buf = new_buf;
        br->buffer_cap = new_cap;
    }

    /* Move unread bytes to the beginning of the owned buffer */
    if (unread > 0) {
        memmove(br->owned_buf, br->owned_buf + br->byte_pos, unread);
    }

    /* Read more data into the remainder of the buffer */
    size_t to_read = br->buffer_cap - unread;
    size_t bytes_read = fread(br->owned_buf + unread, 1, to_read, br->file);

    br->buffer = br->owned_buf;
    br->buffer_size = unread + bytes_read;
    br->byte_pos = 0; /* we've moved unread bytes to start */
    /* br->bit_pos remains the same so partial-byte alignment is preserved */

    if (bytes_read == 0 && unread == 0) {
        /* nothing left to read and nothing unread -> EOF */
        br->overflow = true;
        return false;
    }

    br->overflow = false;
    return true;
}

/* Read up to num_bits (<= 32) from stream. This function will automatically
 * call bitreader_fill_next_chunk() as needed when attached to a file - so
 * callers can just loop bitreader_read until EOF. Returns true on success,
 * false on EOF or unrecoverable error (br->overflow will be true on fatal).
 */
bool bitreader_read(BitReader *br, uint32_t *value, uint8_t num_bits) {
    if (!br || !value) return false;
    if (num_bits > 32) { br->overflow = true; return false; }
    if (num_bits == 0) { *value = 0; return true; }

    uint32_t out = 0;

    for (int i = (int)num_bits - 1; i >= 0; --i) {
        /* ensure at least one byte is available; refill if attached to file */
        while (br->byte_pos >= br->buffer_size) {
            if (br->file) {
                if (!bitreader_fill_next_chunk(br)) {
                    /* EOF */
                    return false;
                }
            } else {
                br->overflow = true;
                return false;
            }
        }

        uint8_t current_byte = br->buffer[br->byte_pos];
        uint8_t bit = (uint8_t)((current_byte >> (7 - br->bit_pos)) & 1u);
        out |= (uint32_t)(bit << i);

        br->bit_pos++;
        if (br->bit_pos == 8) {
            br->bit_pos = 0;
            br->byte_pos++;
        }
    }

    *value = out;
    return true;
}

/* Move reader to next byte boundary preserving invariants. If that moves
 * past the currently loaded buffer and a file is attached, attempt refill. */
void bitreader_move_byte_boundary(BitReader *br) {
    if (!br) return;
    if (br->bit_pos != 0) {
        br->bit_pos = 0;
        br->byte_pos++;

        if (br->byte_pos > br->buffer_size && br->file) {
            /* byte_pos moved past buffer end; refill to restore state */
            (void)bitreader_fill_next_chunk(br);
        }
    }
}

/* Load an entire file into a freshly-allocated buffer (helper utility)
 * Caller must free() the returned buffer. */
uint8_t *bitreader_load_from_file(FILE *fp, size_t *out_size) {
    if (!fp) return NULL;

    if (fseek(fp, 0, SEEK_END) != 0) return NULL;
    long lsize = ftell(fp);
    if (lsize < 0) return NULL;
    size_t size = (size_t)lsize;
    rewind(fp);

    uint8_t *buffer = (uint8_t*)malloc(size);
    if (!buffer) return NULL;

    if (fread(buffer, 1, size, fp) != size) {
        free(buffer);
        return NULL;
    }

    if (out_size) *out_size = size;
    return buffer;
}

/* Peek num_bits from current in-memory buffer without consuming any state.
 * Does NOT attempt to refill from file; returns false if insufficient bits
 * are currently loaded. */
bool bitreader_peek(BitReader *br, uint32_t *value, uint8_t num_bits) {
    if (!br || !value) return false;
    if (num_bits > 32) return false;
    if (num_bits == 0) { *value = 0; return true; }

    size_t local_byte = br->byte_pos;
    uint8_t local_bit = br->bit_pos;
    uint32_t out = 0;

    for (int i = (int)num_bits - 1; i >= 0; --i) {
        if (local_byte >= br->buffer_size) return false; /* don't refill when peeking */
        uint8_t current_byte = br->buffer[local_byte];
        uint8_t bit = (uint8_t)((current_byte >> (7 - local_bit)) & 1u);
        out |= (uint32_t)(bit << i);

        local_bit++;
        if (local_bit == 8) {
            local_bit = 0;
            local_byte++;
        }
    }

    *value = out;
    return true;
}

void bitreader_print_state(const BitReader *br) {
    if (!br) return;
    size_t total_bits = br->byte_pos * 8u + br->bit_pos;
    printf("[BitReader] byte_pos = %zu, bit_pos = %u, total_bits = %zu, overflow = %s, buffer_size = %zu\n",
           br->byte_pos, br->bit_pos, total_bits, br->overflow ? "true" : "false", br->buffer_size);
}

/* Reset to use a new external buffer (non-streaming). Frees any owned_buf. */
void bitreader_reset(BitReader *br, const uint8_t *new_buffer, size_t new_size) {
    if (!br) return;
    if (br->owned_buf) {
        free(br->owned_buf);
        br->owned_buf = NULL;
        br->buffer_cap = 0;
        br->file = NULL;
    }

    br->buffer = new_buffer;
    br->buffer_size = new_size;
    br->byte_pos = 0;
    br->bit_pos = 0;
    br->overflow = false;
}

/* Close the reader and free internal resources (owned_buf). */
void bitreader_close(BitReader *br) {
    if (!br) return;
    if (br->owned_buf) {
        free(br->owned_buf);
        br->owned_buf = NULL;
    }
    br->buffer = NULL;
    br->buffer_size = 0;
    br->byte_pos = 0;
    br->bit_pos = 0;
    br->overflow = false;
    br->file = NULL;
    br->buffer_cap = 0;
}
