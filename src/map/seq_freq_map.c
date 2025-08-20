//seq_freq_map.c

#include "seq_freq_map.h"
#include "xxhash.h"
#include "general_map.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define META_ENCODE(freq, len)  (((uint32_t)(len) << 24) | ((freq) & 0xFFFFFF))
#define META_GET_FREQ(meta)     ((meta) & 0xFFFFFF)
#define META_GET_LEN(meta)      ((uint8_t)((meta) >> 24))

/* Slot states for open addressing with tombstones */
typedef enum {
    SLOT_EMPTY = 0,     // never used
    SLOT_OCCUPIED = 1,  // has a live key
    SLOT_TOMBSTONE = 2  // was used, now deleted
} SlotState;

typedef struct {
    const uint8_t *sequence;  // external pointer (non-owning)
    uint32_t meta;            // upper 8 bits = length, lower 24 bits = frequency
    uint32_t node_id;         // associated node ID
    uint64_t hash;            // cached 64-bit hash of (seq,len)
    uint8_t  state;           // SlotState
} SeqFreqEntry;

typedef struct SeqFreqMap {
    SeqFreqEntry entries[SEQ_MAP_CAPACITY];
} SeqFreqMap;

static SeqFreqMap seqMap;

void init_seq_freq_map(void) {
    memset(&seqMap, 0, sizeof(SeqFreqMap));
    // state is zeroed to SLOT_EMPTY for all entries
}

/*
 * find_slot:
 *   - If key is found: *found=1 and returns index of the OCCUPIED entry.
 *   - If not found: *found=0 and returns index to INSERT:
 *       first TOMBSONE seen on probe (if any), otherwise the first EMPTY slot.
 */
static inline uint32_t find_slot(const uint8_t *seq, uint8_t len, uint64_t hash, int *found) {
    uint32_t idx = (uint32_t)(hash % SEQ_MAP_CAPACITY);
    uint32_t start_idx = idx;
    uint32_t first_tombstone = UINT32_MAX;

    do {
        SeqFreqEntry *entry = &seqMap.entries[idx];

        if (entry->state == SLOT_EMPTY) {
            // Empty stops the search: use a prior tombstone if seen, else this empty.
            *found = 0;
            return (first_tombstone != UINT32_MAX) ? first_tombstone : idx;
        }

        if (entry->state == SLOT_TOMBSTONE) {
            // Remember the first tombstone so we can insert into it later.
            if (first_tombstone == UINT32_MAX) first_tombstone = idx;
        } else { // OCCUPIED
            // Fast checks first: hash -> len -> full compare
            if (entry->hash == hash &&
                META_GET_LEN(entry->meta) == len &&
                sequences_equal(entry->sequence, seq, len)) {
                *found = 1;
                return idx;
            }
        }

        idx = (idx + 1) % SEQ_MAP_CAPACITY;
    } while (idx != start_idx);

    // Table is full of OCCUPIED/TOMBSTONE; if we saw a tombstone, insert there.
    if (first_tombstone != UINT32_MAX) {
        *found = 0;
        return first_tombstone;
    }

    fprintf(stderr, "No free slot in SeqFreqMap\n");
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

uint32_t seq_freq_increment(const uint8_t *seq, uint8_t len, uint32_t node_id) {
    uint64_t hash = XXH3_64bits_withSeed(seq, len, 0);
    int found;
    uint32_t idx = find_slot(seq, len, hash, &found);
    SeqFreqEntry *entry = &seqMap.entries[idx];

    if (found) {
        uint32_t old_freq = META_GET_FREQ(entry->meta);
        uint32_t new_freq = old_freq + 1; // optional: clamp to 24 bits if desired
        entry->meta = META_ENCODE(new_freq, len);
        // policy: keep existing node_id or update; keeping your original 'keep existing' policy here
        return new_freq;
    } else {
        write_new_entry(entry, seq, len, 1, node_id, hash);
        return 1;
    }
}

uint32_t seq_freq_set(const uint8_t *seq, uint8_t len, uint32_t freq, uint32_t node_id) {
    if (freq > 0xFFFFFF) {
        fprintf(stderr, "Frequency exceeds 24-bit limit\n");
        abort();
    }

    uint64_t hash = XXH3_64bits_withSeed(seq, len, 0);
    int found;
    uint32_t idx = find_slot(seq, len, hash, &found);
    SeqFreqEntry *entry = &seqMap.entries[idx];

    write_new_entry(entry, seq, len, freq, node_id, hash);
    return freq;
}

uint32_t seq_freq_decrement(const uint8_t *seq, uint8_t len) {
    uint64_t hash = XXH3_64bits_withSeed(seq, len, 0);
    int found;
    uint32_t idx = find_slot(seq, len, hash, &found);

    if (!found) {
        fprintf(stderr, "Sequence not found for decrement\n");
        abort();
    }

    SeqFreqEntry *entry = &seqMap.entries[idx];
    uint32_t old_freq = META_GET_FREQ(entry->meta);

    if (old_freq == 0) {
        fprintf(stderr, "Invalid decrement — frequency already 0\n");
        abort();
    }

    uint32_t new_freq = old_freq - 1;

    if (old_freq == 1) {
        make_tombstone(entry);  // mark as deleted without breaking probe chains
        return 0;
    } else {
        uint8_t len0 = META_GET_LEN(entry->meta);
        entry->meta = META_ENCODE(new_freq, len0);
        return new_freq;
    }
}

bool seq_freq_get(const uint8_t *seq, uint8_t len, uint32_t *out_freq, uint32_t *out_node_id) {
    uint64_t hash = XXH3_64bits_withSeed(seq, len, 0);
    int found;
    uint32_t idx = find_slot(seq, len, hash, &found);

    if (found) {
        *out_freq = META_GET_FREQ(seqMap.entries[idx].meta);
        *out_node_id = seqMap.entries[idx].node_id;
        return true;
    } else {
        *out_freq = 0;
        *out_node_id = UINT32_MAX; // Sentinel value
        return false;
    }
}

void seq_freq_filter_freqs_and_length(uint32_t freq_to_set, uint32_t freq_to_delete, uint8_t len_to_delete) {
    if (freq_to_set > 0xFFFFFF) {
        fprintf(stderr, "Frequency exceeds 24-bit limit\n");
        abort();
    }

    for (uint32_t i = 0; i < SEQ_MAP_CAPACITY; i++) {
        SeqFreqEntry *entry = &seqMap.entries[i];
        if (entry->state != SLOT_OCCUPIED) continue;

        uint32_t old_freq = META_GET_FREQ(entry->meta);
        uint8_t len = META_GET_LEN(entry->meta);

        if (old_freq <= freq_to_delete || len <= len_to_delete) {
            // Mark as tombstone (do NOT set to empty)
            make_tombstone(entry);
        } else {
            // Reset frequency to the given value, preserve length
            entry->meta = META_ENCODE(freq_to_set, len);
        }
    }
}

uint32_t seq_freq_get_with_index(const uint8_t *seq, uint8_t len, uint32_t *out_freq, uint32_t *out_node_id) {
    uint64_t hash = XXH3_64bits_withSeed(seq, len, 0);
    int found;
    uint32_t idx = find_slot(seq, len, hash, &found);

    if (found) {
        *out_freq = META_GET_FREQ(seqMap.entries[idx].meta);
        *out_node_id = seqMap.entries[idx].node_id;
        return idx;  // success: return usable index
    } else {
        *out_freq = 0;
        *out_node_id = UINT32_MAX;
        return UINT32_MAX;  // failed: not found
    }
}

uint32_t seq_freq_increment_with_index(uint32_t idx, uint32_t node_id) {
    if (idx >= SEQ_MAP_CAPACITY || seqMap.entries[idx].state != SLOT_OCCUPIED) {
        fprintf(stderr, "Invalid or empty index in seq_freq_increment_with_index\n");
        abort();
    }

    SeqFreqEntry *entry = &seqMap.entries[idx];
    uint32_t old_freq = META_GET_FREQ(entry->meta);
    uint8_t len = META_GET_LEN(entry->meta);
    uint32_t new_freq = old_freq + 1;

    entry->meta = META_ENCODE(new_freq, len);
    entry->node_id = node_id;  // Update node_id on increment

    return new_freq;
}

uint32_t seq_freq_decrement_with_index(uint32_t idx) {
    if (idx >= SEQ_MAP_CAPACITY || seqMap.entries[idx].state != SLOT_OCCUPIED) {
        fprintf(stderr, "Invalid or empty index in seq_freq_decrement_with_index\n");
        abort();
    }

    SeqFreqEntry *entry = &seqMap.entries[idx];
    uint32_t old_freq = META_GET_FREQ(entry->meta);

    if (old_freq == 0) {
        fprintf(stderr, "Invalid decrement — frequency already 0\n");
        abort();
    }

    uint8_t len = META_GET_LEN(entry->meta);
    uint32_t new_freq = old_freq - 1;

    if (old_freq == 1) {
        make_tombstone(entry);
        return 0;
    } else {
        entry->meta = META_ENCODE(new_freq, len);
        return new_freq;
    }
}

uint32_t seq_freq_set_existing(uint32_t idx, uint32_t freq, uint32_t node_id) {
    if (idx >= SEQ_MAP_CAPACITY || seqMap.entries[idx].state != SLOT_OCCUPIED) {
        fprintf(stderr, "Invalid or empty index in seq_freq_set_existing\n");
        abort();
    }
    if (freq > 0xFFFFFF) {
        fprintf(stderr, "Frequency exceeds 24-bit limit\n");
        abort();
    }

    SeqFreqEntry *entry = &seqMap.entries[idx];
    uint8_t len = META_GET_LEN(entry->meta);

    entry->meta = META_ENCODE(freq, len);
    entry->node_id = node_id;

    return freq;
}

void seq_freq_map_print(void) {
    printf("---- SeqFreqMap Contents ----\n");
    for (uint32_t i = 0; i < SEQ_MAP_CAPACITY; i++) {
        const SeqFreqEntry *entry = &seqMap.entries[i];
        if (entry->state == SLOT_OCCUPIED) {
            uint8_t len = META_GET_LEN(entry->meta);
            uint32_t freq = META_GET_FREQ(entry->meta);
            printf("[%04u] freq=%u, len=%u, node_id=%u, seq=", i, freq, len, entry->node_id);
            for (uint8_t j = 0; j < len; j++) {
                printf("%c", entry->sequence[j]);
            }
            printf("\n");
        }
    }
    printf("-----------------------------\n");
}
