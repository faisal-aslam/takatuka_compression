//seq_freq_map.c

#include "seq_freq_map.h"
#include "general_map.h" /* for sequences_equal (must be provided elsewhere) */
#include "xxhash.h"      /* defines XXH3_64bits_withSeed */

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

/* Meta: upper 8 bits = length, lower 24 bits = frequency */
#define META_ENCODE(freq, len) (((uint32_t)(len) << 24) | ((freq) & 0xFFFFFF))
#define META_GET_FREQ(meta) ((meta) & 0xFFFFFF)
#define META_GET_LEN(meta)  ((uint8_t)((meta) >> 24))

/* Slot states for open addressing */
typedef enum { SLOT_EMPTY = 0, SLOT_OCCUPIED = 1, SLOT_TOMBSTONE = 2 } SlotState;

typedef struct {
    const uint8_t *sequence; /* external pointer — non-owning */
    uint32_t meta;           /* encoded len/freq */
    uint32_t node_id;
    uint64_t hash; /* cached hash for fast compares */
    uint8_t state; /* SlotState */
} SeqFreqEntry;

typedef struct {
    SeqFreqEntry entries[SEQ_MAP_CAPACITY];
} SeqFreqMap;

static SeqFreqMap seqMap;

/* Best cache (lazy): index into seqMap.entries or UINT32_MAX when invalid */
static uint32_t best_sequence_index;
static double   best_sequence_saving;

/* --------------------------------------------------------- */
/* Helpers */

static inline __attribute__((always_inline))
double compute_saving_from_meta(uint32_t meta) {
    uint32_t freq = META_GET_FREQ(meta);
    uint32_t len  = META_GET_LEN(meta);

    if (freq == 0 || len == 0) return DBL_MAX;  /* treat invalid as very large (never chosen) */

    /* Your formula: saving = (freq + len) - ((len - 1) * freq) */
    return (double)((freq + len) - ((len - 1) * freq));
}

static inline void invalidate_best(void) {
    best_sequence_index = UINT32_MAX;
    best_sequence_saving = DBL_MAX;  /* we are minimizing */
}

/* update best if entry idx is occupied and has strictly smaller saving */
static inline void update_best_for_index(uint32_t idx) {
    if (idx >= SEQ_MAP_CAPACITY) return;
    SeqFreqEntry *e = &seqMap.entries[idx];
    if (e->state != SLOT_OCCUPIED) return;
    double s = compute_saving_from_meta(e->meta);
    if (s < best_sequence_saving) {
        best_sequence_saving = s;
        best_sequence_index = idx;
    }
}

/* --------------------------------------------------------- */
/* find_slot:
 *   If found: *found = 1 and returns index of the occupied entry.
 *   If not found: *found = 0 and returns index to insert (first tombstone or an empty).
 */
static inline uint32_t find_slot(const uint8_t *seq, uint8_t len, uint64_t hash, int *found) {
    uint32_t idx = (uint32_t)(hash % SEQ_MAP_CAPACITY);
    uint32_t start_idx = idx;
    uint32_t first_tombstone = UINT32_MAX;

    do {
        SeqFreqEntry *entry = &seqMap.entries[idx];

        if (entry->state == SLOT_EMPTY) {
            *found = 0;
            return (first_tombstone != UINT32_MAX) ? first_tombstone : idx;
        }

        if (entry->state == SLOT_TOMBSTONE) {
            if (first_tombstone == UINT32_MAX) first_tombstone = idx;
        } else { /* OCCUPIED */
            if (entry->hash == hash && META_GET_LEN(entry->meta) == len &&
                sequences_equal(entry->sequence, seq, len)) {
                *found = 1;
                return idx;
            }
        }

        idx = (idx + 1) % SEQ_MAP_CAPACITY;
    } while (idx != start_idx);

    if (first_tombstone != UINT32_MAX) {
        *found = 0;
        return first_tombstone;
    }

    fprintf(stderr, "SeqFreqMap is full (no free slot)\n");
    abort();
}

static inline void write_new_entry(SeqFreqEntry *entry,
                                   const uint8_t *seq, uint8_t len,
                                   uint32_t freq, uint32_t node_id,
                                   uint64_t hash) {
    entry->sequence = seq;
    entry->meta     = META_ENCODE(freq, len);
    entry->node_id  = node_id;
    entry->hash     = hash;
    entry->state    = SLOT_OCCUPIED;
}

static inline void make_tombstone(SeqFreqEntry *entry) {
    entry->sequence = NULL;
    entry->meta     = 0;
    entry->node_id  = 0;
    entry->hash     = 0;
    entry->state    = SLOT_TOMBSTONE;
}

/* --------------------------------------------------------- */
/* Public API */

void init_seq_freq_map(void) {
    memset(&seqMap, 0, sizeof(SeqFreqMap));
    invalidate_best();
}

uint32_t seq_freq_increment(const uint8_t *seq, uint8_t len, uint32_t node_id) {
    uint64_t hash = XXH3_64bits_withSeed(seq, len, 0);
    int found;
    uint32_t idx = find_slot(seq, len, hash, &found);
    SeqFreqEntry *entry = &seqMap.entries[idx];

    if (found) {
        uint32_t old_freq = META_GET_FREQ(entry->meta);
        uint32_t new_freq = old_freq + 1;
        entry->meta = META_ENCODE(new_freq, len);
        entry->node_id = node_id;
        update_best_for_index(idx);
        return new_freq;
    } else {
        write_new_entry(entry, seq, len, 1, node_id, hash);
        update_best_for_index(idx);
        return 1;
    }
}

uint32_t seq_freq_set(const uint8_t *seq, uint8_t len,
                      uint32_t freq, uint32_t node_id) {
    if (freq > 0xFFFFFF) {
        fprintf(stderr, "seq_freq_set: freq exceeds 24-bit limit\n");
        abort();
    }

    uint64_t hash = XXH3_64bits_withSeed(seq, len, 0);
    int found;
    uint32_t idx = find_slot(seq, len, hash, &found);
    SeqFreqEntry *entry = &seqMap.entries[idx];

    if (found) {
        double old_saving = compute_saving_from_meta(entry->meta);
        entry->meta = META_ENCODE(freq, len);
        entry->node_id = node_id;
        double new_saving = compute_saving_from_meta(entry->meta);

        if (new_saving < best_sequence_saving) {
            best_sequence_saving = new_saving;
            best_sequence_index = idx;
        } else if (idx == best_sequence_index && new_saving > old_saving) {
            invalidate_best();
        }
    } else {
        write_new_entry(entry, seq, len, freq, node_id, hash);
        update_best_for_index(idx);
    }
    return freq;
}

uint32_t seq_freq_decrement(const uint8_t *seq, uint8_t len) {
    uint64_t hash = XXH3_64bits_withSeed(seq, len, 0);
    int found;
    uint32_t idx = find_slot(seq, len, hash, &found);

    if (!found) {
        fprintf(stderr, "seq_freq_decrement: sequence not found\n");
        abort();
    }

    SeqFreqEntry *entry = &seqMap.entries[idx];
    uint32_t old_freq = META_GET_FREQ(entry->meta);

    if (old_freq == 0) {
        fprintf(stderr, "seq_freq_decrement: frequency already 0\n");
        abort();
    }

    if (old_freq == 1) {
        make_tombstone(entry);
        if (idx == best_sequence_index) invalidate_best();
        return 0;
    } else {
        uint8_t old_len = META_GET_LEN(entry->meta);
        entry->meta = META_ENCODE(old_freq - 1, old_len);
        if (idx == best_sequence_index) invalidate_best();
        return old_freq - 1;
    }
}

bool seq_freq_get(const uint8_t *seq, uint8_t len,
                  uint32_t *out_freq, uint32_t *out_node_id) {
    uint64_t hash = XXH3_64bits_withSeed(seq, len, 0);
    int found;
    uint32_t idx = find_slot(seq, len, hash, &found);

    if (found) {
        if (out_freq) *out_freq = META_GET_FREQ(seqMap.entries[idx].meta);
        if (out_node_id) *out_node_id = seqMap.entries[idx].node_id;
        return true;
    } else {
        if (out_freq) *out_freq = 0;
        if (out_node_id) *out_node_id = UINT32_MAX;
        return false;
    }
}

void seq_freq_filter_freqs_and_length(uint32_t freq_to_set,
                                      uint32_t freq_to_delete,
                                      uint8_t len_to_delete) {
    if (freq_to_set > 0xFFFFFF) {
        fprintf(stderr, "freq_to_set exceeds 24-bit limit\n");
        abort();
    }

    for (uint32_t i = 0; i < SEQ_MAP_CAPACITY; i++) {
        SeqFreqEntry *entry = &seqMap.entries[i];
        if (entry->state != SLOT_OCCUPIED) continue;

        uint32_t old_freq = META_GET_FREQ(entry->meta);
        uint8_t len = META_GET_LEN(entry->meta);

        if (old_freq <= freq_to_delete || len <= len_to_delete) {
            make_tombstone(entry);
        } else {
            entry->meta = META_ENCODE(freq_to_set, len);
        }
    }

    invalidate_best();
}

uint32_t seq_freq_get_with_index(const uint8_t *seq, uint8_t len,
                                 uint32_t *out_freq, uint32_t *out_node_id) {
    uint64_t hash = XXH3_64bits_withSeed(seq, len, 0);
    int found;
    uint32_t idx = find_slot(seq, len, hash, &found);
    if (found) {
        if (out_freq) *out_freq = META_GET_FREQ(seqMap.entries[idx].meta);
        if (out_node_id) *out_node_id = seqMap.entries[idx].node_id;
        return idx;
    } else {
        if (out_freq) *out_freq = 0;
        if (out_node_id) *out_node_id = UINT32_MAX;
        return UINT32_MAX;
    }
}

uint32_t seq_freq_increment_with_index(uint32_t idx, uint32_t node_id) {
    if (idx >= SEQ_MAP_CAPACITY || seqMap.entries[idx].state != SLOT_OCCUPIED) {
        fprintf(stderr, "seq_freq_increment_with_index: invalid index\n");
        abort();
    }

    SeqFreqEntry *entry = &seqMap.entries[idx];
    uint32_t old_freq = META_GET_FREQ(entry->meta);
    uint8_t len = META_GET_LEN(entry->meta);
    uint32_t new_freq = old_freq + 1;
    entry->meta = META_ENCODE(new_freq, len);
    entry->node_id = node_id;

    update_best_for_index(idx);
    return new_freq;
}

uint32_t seq_freq_decrement_with_index(uint32_t idx) {
    if (idx >= SEQ_MAP_CAPACITY || seqMap.entries[idx].state != SLOT_OCCUPIED) {
        fprintf(stderr, "seq_freq_decrement_with_index: invalid index\n");
        abort();
    }

    SeqFreqEntry *entry = &seqMap.entries[idx];
    uint32_t old_freq = META_GET_FREQ(entry->meta);

    if (old_freq == 0) {
        fprintf(stderr, "seq_freq_decrement_with_index: frequency already 0\n");
        abort();
    }

    if (old_freq == 1) {
        make_tombstone(entry);
        if (idx == best_sequence_index) invalidate_best();
        return 0;
    } else {
        uint8_t len = META_GET_LEN(entry->meta);
        entry->meta = META_ENCODE(old_freq - 1, len);
        if (idx == best_sequence_index) invalidate_best();
        return old_freq - 1;
    }
}

uint32_t seq_freq_set_existing(uint32_t idx,
                               uint32_t freq, uint32_t node_id) {
    if (idx >= SEQ_MAP_CAPACITY || seqMap.entries[idx].state != SLOT_OCCUPIED) {
        fprintf(stderr, "seq_freq_set_existing: invalid index\n");
        abort();
    }
    if (freq > 0xFFFFFF) {
        fprintf(stderr, "seq_freq_set_existing: freq exceeds 24-bit limit\n");
        abort();
    }

    SeqFreqEntry *entry = &seqMap.entries[idx];
    double old_saving = compute_saving_from_meta(entry->meta);
    uint8_t len = META_GET_LEN(entry->meta);
    entry->meta = META_ENCODE(freq, len);
    entry->node_id = node_id;

    double new_saving = compute_saving_from_meta(entry->meta);
    if (new_saving < best_sequence_saving) {
        best_sequence_saving = new_saving;
        best_sequence_index = idx;
    } else if (idx == best_sequence_index && new_saving > old_saving) {
        invalidate_best();
    }

    return freq;
}

void seq_freq_map_print(void) {
    printf("---- SeqFreqMap Contents ----\n");
    for (uint32_t i = 0; i < SEQ_MAP_CAPACITY; i++) {
        const SeqFreqEntry *entry = &seqMap.entries[i];
        if (entry->state == SLOT_OCCUPIED) {
            uint8_t len = META_GET_LEN(entry->meta);
            uint32_t freq = META_GET_FREQ(entry->meta);
            double s = compute_saving_from_meta(entry->meta);
            printf("[%04u] freq=%u, len=%u, node_id=%u, saving=%.2f, seq=",
                   i, freq, len, entry->node_id, s);
            for (uint8_t j = 0; j < len; j++)
                putchar(entry->sequence[j]);
            putchar('\n');
        }
    }
    printf("-----------------------------\n");
}

/* --------------------------------------------------------- */
/* Best-cache API */

void seq_freq_recompute_best(void) {
    best_sequence_index = UINT32_MAX;
    best_sequence_saving = DBL_MAX;

    for (uint32_t i = 0; i < SEQ_MAP_CAPACITY; i++) {
        SeqFreqEntry *entry = &seqMap.entries[i];
        if (entry->state != SLOT_OCCUPIED) continue;
        double s = compute_saving_from_meta(entry->meta);
        if (s < best_sequence_saving) {
            best_sequence_saving = s;
            best_sequence_index = i;
        }
    }
}

bool seq_freq_best_is_valid(void) {
    return best_sequence_index != UINT32_MAX &&
           best_sequence_index < SEQ_MAP_CAPACITY &&
           seqMap.entries[best_sequence_index].state == SLOT_OCCUPIED;
}

uint32_t seq_freq_get_best_index(void) { return best_sequence_index; }

bool seq_freq_get_best(const uint8_t **out_seq, uint8_t *out_len,
                       uint32_t *out_freq, uint32_t *out_node_id) {
    if (!seq_freq_best_is_valid()) {
        if (out_seq) *out_seq = NULL;
        if (out_len) *out_len = 0;
        if (out_freq) *out_freq = 0;
        if (out_node_id) *out_node_id = UINT32_MAX;
        return false;
    }

    SeqFreqEntry *e = &seqMap.entries[best_sequence_index];
    if (out_seq) *out_seq = e->sequence;
    if (out_len) *out_len = META_GET_LEN(e->meta);
    if (out_freq) *out_freq = META_GET_FREQ(e->meta);
    if (out_node_id) *out_node_id = e->node_id;
    return true;
}
