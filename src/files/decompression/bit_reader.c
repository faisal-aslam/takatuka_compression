// bit_reader.c

#include "bit_reader.h"
#include <stdlib.h>
#include <string.h>

/* ---------- Internal helpers ---------- */

static bool br_has_file(const BitReader *br) {
    return br && br->file != NULL && br->owned_buf != NULL && br->buffer_cap > 0;
}

/* Load (or reload) the internal buffer from file. Resets byte/bit positions. */
static bool br_reload(BitReader *br) {
    if (!br_has_file(br)) return false;

    size_t n = fread(br->owned_buf, 1, br->buffer_cap, br->file);
    if (n == 0) {
        /* True EOF (or error). Set sticky overflow to signal no more data. */
        br->overflow = true;
        return false;
    }

    br->buffer = br->owned_buf;
    br->buffer_size = n;
    br->byte_pos = 0;
    br->bit_pos = 0;
    /* Do not set overflow=false here; leave it as-is only if it was previously clear. */
    return true;
}

/* ---------- Public API ---------- */

void bitreader_init(BitReader *br, const uint8_t *buffer, size_t size) {
    if (!br) return;
    br->buffer = buffer;
    br->buffer_size = size;
    br->byte_pos = 0;
    br->bit_pos = 0;
    br->overflow = false;

    /* Ensure all optional fields are well-defined to avoid invalid frees later */
    br->file = NULL;
    br->owned_buf = NULL;
    br->buffer_cap = 0;
}

void bitreader_attach_file(BitReader *br, FILE *file, size_t buffer_cap) {
    if (!br || !file || buffer_cap == 0) {
        fprintf(stderr, "bitreader_attach_file: invalid args\n");
        exit(EXIT_FAILURE);
    }

    /* If previously attached, clean up owned buffer */
    if (br && br->file && br->owned_buf && br->buffer_cap > 0) {
        free(br->owned_buf);
    }
    br->owned_buf = NULL;

    br->owned_buf = (uint8_t *)malloc(buffer_cap);
    if (!br->owned_buf) {
        fprintf(stderr, "bitreader_attach_file: OOM allocating %zu bytes\n", buffer_cap);
        exit(EXIT_FAILURE);
    }

    br->file = file;
    br->buffer_cap = buffer_cap;
    br->overflow = false;

    /* Preload first chunk */
    if (!br_reload(br)) {
        /* On immediate EOF, we keep state consistent but mark overflow so reads return false */
        br->buffer = br->owned_buf; /* valid pointer even if empty */
        br->buffer_size = 0;
        br->byte_pos = 0;
        br->bit_pos = 0;
    }
}

bool bitreader_read(BitReader *br, uint32_t *value, uint8_t num_bits) {
    if (!br || !value) return false;
    if (num_bits > 32) return false;
    if (num_bits == 0) {
        *value = 0u;
        return true;
    }
    if (br->overflow) return false;

    /* Save state for atomicity (restore on failure) */
    const size_t save_byte_pos = br->byte_pos;
    const uint8_t save_bit_pos = br->bit_pos;
    const bool save_overflow = br->overflow;

    uint32_t out = 0;

    for (int i = (int)num_bits - 1; i >= 0; --i) {
        /* Need a byte to read a bit from */
        while (br->byte_pos >= br->buffer_size) {
            /* buffer exhausted: try to refill if we have a file */
            if (br_has_file(br)) {
                if (!br_reload(br)) {
                    /* EOF: restore and fail atomically */
                    br->byte_pos = save_byte_pos;
                    br->bit_pos = save_bit_pos;
                    br->overflow = true; /* sticky EOF */
                    return false;
                }
            } else {
                /* no file backing; true overflow */
                br->byte_pos = save_byte_pos;
                br->bit_pos = save_bit_pos;
                br->overflow = true;
                return false;
            }
        }

        const uint8_t cur = br->buffer[br->byte_pos];
        const uint8_t bit = (uint8_t)((cur >> (7u - br->bit_pos)) & 1u);
        out |= ((uint32_t)bit) << i;

        /* advance one bit */
        br->bit_pos++;
        if (br->bit_pos == 8) {
            br->bit_pos = 0;
            br->byte_pos++;
        }
    }

    *value = out;
    return true;
}

bool bitreader_peek(BitReader *br, uint32_t *value, uint8_t num_bits) {
    if (!br || !value) return false;
    if (num_bits > 32) return false;
    if (num_bits == 0) {
        *value = 0u;
        return true;
    }
    if (br->overflow) return false;

    /* Save state, do NOT auto-refill for peek */
    const size_t save_byte_pos = br->byte_pos;
    const uint8_t save_bit_pos = br->bit_pos;
    const bool save_overflow = br->overflow;

    uint32_t out = 0;

    for (int i = (int)num_bits - 1; i >= 0; --i) {
        if (br->byte_pos >= br->buffer_size) {
            /* Not enough data in the currently loaded buffer */
            br->byte_pos = save_byte_pos;
            br->bit_pos = save_bit_pos;
            br->overflow = save_overflow;
            return false;
        }
        const uint8_t cur = br->buffer[br->byte_pos];
        const uint8_t bit = (uint8_t)((cur >> (7u - br->bit_pos)) & 1u);
        out |= ((uint32_t)bit) << i;

        /* advance one bit locally */
        br->bit_pos++;
        if (br->bit_pos == 8) {
            br->bit_pos = 0;
            br->byte_pos++;
        }
    }

    /* Restore state */
    br->byte_pos = save_byte_pos;
    br->bit_pos = save_bit_pos;
    br->overflow = save_overflow;

    *value = out;
    return true;
}

void bitreader_move_byte_boundary(BitReader *br) {
    if (!br) return;
    if (br->bit_pos != 0) {
        br->bit_pos = 0;
        br->byte_pos++;
    }
    /* If we landed past the end of the loaded buffer, the next read() will refill automatically. */
}

uint8_t *bitreader_load_from_file(FILE *fp, size_t *out_size) {
    if (!fp) return NULL;

    /* Use fseeko/ftello when available; fallback to ftell on platforms without large-file API */
#if defined(_WIN32) || defined(_WIN64)
    /* Windows ftell is 32-bit on old MSVCRT; assume modern runtime or large-file not critical here */
    if (fseek(fp, 0, SEEK_END) != 0) return NULL;
    long sz = ftell(fp);
    if (sz < 0) return NULL;
    if (fseek(fp, 0, SEEK_SET) != 0) return NULL;
    size_t size = (size_t)sz;
#else
    if (fseeko(fp, 0, SEEK_END) != 0) return NULL;
    off_t sz = ftello(fp);
    if (sz < 0) return NULL;
    if (fseeko(fp, 0, SEEK_SET) != 0) return NULL;
    size_t size = (size_t)sz;
#endif

    uint8_t *buf = (uint8_t *)malloc(size ? size : 1); /* malloc(0) is implementation-defined */
    if (!buf) return NULL;

    size_t n = fread(buf, 1, size, fp);
    if (n != size) {
        free(buf);
        return NULL;
    }

    if (out_size) *out_size = size;
    return buf;
}

bool bitreader_fill_next_chunk(BitReader *br) {
    if (!br) return false;
    if (!br_has_file(br)) return false;

    /* Only allow manual refill when we are exactly at a byte boundary;
       if not, the caller should first call bitreader_move_byte_boundary(). */
    if (br->bit_pos != 0) {
        /* Not an error; just refuse to refill mid-byte to avoid state confusion. */
        return false;
    }

    return br_reload(br);
}

void bitreader_print_state(const BitReader *br) {
    if (!br) return;
    printf("[BitReader] byte_pos=%zu bit_pos=%u buffer_size=%zu overflow=%s file=%p cap=%zu\n", br->byte_pos,
           br->bit_pos, br->buffer_size, br->overflow ? "true" : "false", (void *)br->file, br->buffer_cap);
}

void bitreader_reset(BitReader *br, const uint8_t *new_buffer, size_t new_size) {
    if (!br) return;

    /* If previously attached to a file, keep ownership of owned_buf, but switch to external buffer mode. */
    br->buffer = new_buffer;
    br->buffer_size = new_size;
    br->byte_pos = 0;
    br->bit_pos = 0;
    br->overflow = false;

    /* Detach file semantics (do not free here) */
    br->file = NULL;
    br->buffer_cap = 0;
    /* Keep owned_buf allocated in case user re-attaches; but since file is NULL now, we won't free/use it. */
    if (br->owned_buf) {
        free(br->owned_buf);
        br->owned_buf = NULL;
    }
}

void bitreader_close(BitReader *br) {
    if (!br) return;

    if (br->owned_buf) {
        free(br->owned_buf);
        br->owned_buf = NULL;
    }

    /* Do not fclose(br->file) here — the caller owns the FILE* */
    br->buffer = NULL;
    br->buffer_size = 0;
    br->byte_pos = 0;
    br->bit_pos = 0;
    br->overflow = false;

    br->file = NULL;
    br->buffer_cap = 0;
}
