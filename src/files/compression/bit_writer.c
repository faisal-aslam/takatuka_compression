// bit_writer.c — robust, all-or-nothing bitstream writer with growable buffer.

#include "bit_writer.h"
#include <stdlib.h>
#include <string.h>

/* ---------- internal helpers ---------- */

static inline size_t bw_total_bits(const BitWriter *bw) { return (bw->byte_pos * 8u) + (size_t)bw->bit_pos; }

static bool bw_reserve_bytes(BitWriter *bw, size_t needed_bytes) {
    if (needed_bytes <= bw->_cap) return true;

    size_t new_cap = bw->_cap ? bw->_cap : (bw->buffer_size ? bw->buffer_size : 4096u);
    while (new_cap < needed_bytes) {
        size_t doubled = new_cap * 2u;
        if (doubled < new_cap) { // overflow guard
            new_cap = needed_bytes;
            break;
        }
        new_cap = doubled;
    }

    uint8_t *new_data = (uint8_t *)realloc(bw->_data, new_cap);
    if (!new_data) return false;

    /* zero the newly added portion to keep bitwise ops deterministic */
    if (new_cap > bw->_cap) {
        memset(new_data + bw->_cap, 0, new_cap - bw->_cap);
    }

    bw->_data = new_data;
    bw->_cap = new_cap;
    return true;
}

static inline void bw_update_sizes_after_cursor_move(BitWriter *bw) {
    /* _size should reflect the number of whole bytes that contain any data */
    size_t used = bw->byte_pos + (bw->bit_pos != 0 ? 1u : 0u);
    if (used > bw->_size) {
        /* zero the new byte if we just extended into it */
        if (bw->_data && used > 0 && used > bw->_size) {
            /* ensure the current target byte is zeroed (already zeroed in reserve, but safe) */
            /* not strictly needed due to reserve zeroing, but harmless */
        }
        bw->_size = used;
    }
}

/* ---------- public API ---------- */

void bitwriter_init(BitWriter *bw, uint8_t *buffer, size_t size) {
    if (!bw) return;

    bw->buffer = buffer;    /* legacy, not used for storage */
    bw->buffer_size = size; /* legacy, not used for storage */
    bw->byte_pos = 0;
    bw->bit_pos = 0;
    bw->overflow = false;

    bw->_data = NULL;
    bw->_cap = 0;
    bw->_size = 0;

    /* pre-reserve based on provided size (or 4096) */
    size_t initial = size ? size : 4096u;
    if (bw_reserve_bytes(bw, initial)) {
        /* zero initial area */
        memset(bw->_data, 0, bw->_cap);
    }
#ifdef DEBUG
    /* mirror original behavior: zero the legacy external buffer if provided */
    if (buffer && size) {
        memset(buffer, 0, size);
    }
#endif
}

bool bitwriter_write(BitWriter *bw, uint32_t value, uint8_t num_bits
#ifdef DEBUG
                     ,
                     const char *label
#endif
) {
    if (!bw) return false;
    if (num_bits == 0) return true;
    if (num_bits > 32) {
        bw->overflow = true;
        return false;
    }
#ifdef DEBUG
    FILE *log = fopen("log.txt", "a");
    if (log) {
        if (label) fprintf(log, "[BitWriter] %s: ", label);
        else
            fprintf(log, "[BitWriter] ");
        fprintf(log, "Writing %u bits: ", num_bits);
    }
#endif

    /* compute target capacity in bytes BEFORE writing to ensure all-or-nothing */
    size_t start_bits = bw_total_bits(bw);
    size_t target_bits = start_bits + (size_t)num_bits;
    size_t needed_bytes = (target_bits + 7u) / 8u;
    if (!bw_reserve_bytes(bw, needed_bytes)) {
#ifdef DEBUG
        if (log) {
            fprintf(log, " (ALLOC FAIL)\n");
            fclose(log);
        }
#endif
        return false;
    }

    /* perform the write, guaranteed to fit */
    for (int i = (int)num_bits - 1; i >= 0; --i) {
        uint8_t bit = (uint8_t)((value >> i) & 1u);

        /* ensure the target byte exists (reserve already zeroed it) */
        if (bit) {
            bw->_data[bw->byte_pos] |= (uint8_t)(1u << (7 - bw->bit_pos));
        } else {
            bw->_data[bw->byte_pos] &= (uint8_t)~(1u << (7 - bw->bit_pos));
        }

#ifdef DEBUG
        if (log) fputc(bit ? '1' : '0', log);
#endif

        bw->bit_pos++;
        if (bw->bit_pos == 8) {
            bw->bit_pos = 0;
            bw->byte_pos++;
        }
    }

    bw_update_sizes_after_cursor_move(bw);

#ifdef DEBUG
    if (log) {
        fputc('\n', log);
        fclose(log);
    }
#endif
    return true;
}

size_t bitwriter_bytes_written(const BitWriter *bw) { return bw ? bw->_size : 0u; }

bool bitwriter_write_to_file(const BitWriter *bw_in, FILE *fp) {
    if (!bw_in || !fp) return false;

    /* We need a non-const handle to reset/clear internal storage after writing. */
    BitWriter *bw = (BitWriter *)bw_in;

    size_t bytes_to_write = bitwriter_bytes_written(bw);
    if (bytes_to_write == 0) return true;

    size_t written = fwrite(bw->_data, 1, bytes_to_write, fp);
    if (written != bytes_to_write) {
        return false;
    }

    /* After a successful flush, clear the stream and release memory to avoid leaks. */
    bw->byte_pos = 0;
    bw->bit_pos = 0;
    bw->overflow = false;

    if (bw->_data) {
        free(bw->_data);
        bw->_data = NULL;
    }
    bw->_cap = 0;
    bw->_size = 0;

    return true;
}

void bitwriter_print_state(const BitWriter *bw) {
    if (!bw) return;
    size_t total_bits = bw_total_bits(bw);
    printf("[BitWriter] byte_pos = %zu, bit_pos = %u, total_bits = %zu, overflow = %s, bytes_buffered = %zu\n",
           bw->byte_pos, bw->bit_pos, total_bits, bw->overflow ? "true" : "false", bitwriter_bytes_written(bw));
}

bool bitwriter_overwrite_at(BitWriter *bw, size_t bit_pos, uint32_t value, uint8_t num_bits) {
    if (!bw) return false;
    if (num_bits == 0) return true;
    if (num_bits > 32) {
        bw->overflow = true;
        return false;
    }

    size_t total_stream_bits = bw->_size * 8u;
    if (bit_pos + (size_t)num_bits > total_stream_bits) {
        /* cannot patch beyond current stream end */
        return false;
    }

    for (int i = (int)num_bits - 1; i >= 0; --i) {
        size_t current_bit = bit_pos + (size_t)(num_bits - 1 - i);
        size_t byte_index = current_bit / 8u;
        size_t bit_index = 7u - (current_bit % 8u); // MSB-first
        uint8_t bit = (uint8_t)((value >> i) & 1u);

        if (bit) bw->_data[byte_index] |= (uint8_t)(1u << bit_index);
        else
            bw->_data[byte_index] &= (uint8_t)~(1u << bit_index);
    }
    return true;
}

void bitwriter_reset(BitWriter *bw) {
    if (!bw) return;
    bw->byte_pos = 0;
    bw->bit_pos = 0;
    bw->overflow = false;

    if (bw->_data) {
        free(bw->_data);
        bw->_data = NULL;
    }
    bw->_cap = 0;
    bw->_size = 0;

    /* keep legacy external buffer zeroed if provided; it is not used for storage */
    if (bw->buffer && bw->buffer_size) {
        memset(bw->buffer, 0, bw->buffer_size);
    }
}

void bitwriter_reset_positions(BitWriter *bw) {
    if (!bw) return;
    /* clear stream but keep capacity for reuse */
    bw->byte_pos = 0;
    bw->bit_pos = 0;
    bw->overflow = false;

    if (bw->_data && bw->_size) {
        /* zero the used region for cleanliness */
        memset(bw->_data, 0, bw->_size);
    }
    bw->_size = 0;
}
