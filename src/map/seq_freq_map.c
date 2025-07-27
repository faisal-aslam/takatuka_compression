//seq_freq_map.c

#include "seq_freq_map.h"
#include "xxhash.h"
#include "general_map.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define META_ENCODE(freq, len)  (((uint32_t)(len) << 24) | ((freq) & 0xFFFFFF))
#define META_GET_FREQ(meta)     ((meta) & 0xFFFFFF)
#define META_GET_LEN(meta)      ((uint8_t)((meta) >> 24))

static uint32_t freq_one_count = 0;

typedef struct {
    const uint8_t *sequence;  // external pointer (non-owning)
    uint32_t meta;            // upper 8 bits = length, lower 24 bits = frequency
    uint32_t node_id;         // associated node ID
} SeqFreqEntry;

typedef struct SeqFreqMap {
    SeqFreqEntry entries[SEQ_MAP_CAPACITY];
} SeqFreqMap;

static SeqFreqMap seqMap;

void init_seq_freq_map(void) {
    memset(&seqMap, 0, sizeof(SeqFreqMap));
    freq_one_count = 0;
}

static inline uint32_t find_slot(const uint8_t *seq, uint8_t len, uint64_t hash, int *found) {
    uint32_t idx = hash % SEQ_MAP_CAPACITY;
    uint32_t start_idx = idx;

    do {
        const SeqFreqEntry *entry = &seqMap.entries[idx];

        if (entry->sequence == NULL) {
            *found = 0;
            return idx;
        }

        if (META_GET_LEN(entry->meta) == len &&
            sequences_equal(entry->sequence, seq, len)) {
            *found = 1;
            return idx;
        }

        idx = (idx + 1) % SEQ_MAP_CAPACITY;
    } while (idx != start_idx);

    fprintf(stderr, "No free slot in SeqFreqMap\n");
    abort();
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

        if (old_freq == 1) freq_one_count--;
        return new_freq;
    } else {
        entry->sequence = seq;
        entry->meta = META_ENCODE(1, len);
        entry->node_id = node_id;
        freq_one_count++;
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
    uint32_t old_freq = found ? META_GET_FREQ(entry->meta) : 0;

    entry->sequence = seq;
    entry->meta = META_ENCODE(freq, len);
    entry->node_id = node_id;

    if (old_freq == 1 && freq != 1) freq_one_count--;
    else if (old_freq != 1 && freq == 1) freq_one_count++;
    else if (!found && freq == 1) freq_one_count++;

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
        freq_one_count--;  // from 1 to 0
        entry->sequence = NULL;
        entry->meta = 0;
        entry->node_id = 0;
        return 0;
    } else {
        if (new_freq == 1) freq_one_count++;  // from >1 to 1
        entry->meta = META_ENCODE(new_freq, len);
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

uint32_t seq_freq_one_count(void) {
    return freq_one_count;
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
    if (idx >= SEQ_MAP_CAPACITY || seqMap.entries[idx].sequence == NULL) {
        fprintf(stderr, "Invalid or empty index in seq_freq_increment_with_index\n");
        abort();
    }

    SeqFreqEntry *entry = &seqMap.entries[idx];
    uint32_t old_freq = META_GET_FREQ(entry->meta);
    uint8_t len = META_GET_LEN(entry->meta);
    uint32_t new_freq = old_freq + 1;

    entry->meta = META_ENCODE(new_freq, len);
    entry->node_id = node_id;  // Update node_id on increment

    if (old_freq == 1) freq_one_count--;  // went from 1 to >1

    return new_freq;
}


uint32_t seq_freq_decrement_with_index(uint32_t idx) {
    if (idx >= SEQ_MAP_CAPACITY || seqMap.entries[idx].sequence == NULL) {
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
        freq_one_count--;  // from 1 to 0
        entry->sequence = NULL;
        entry->meta = 0;
        entry->node_id = 0;
        return 0;
    } else {
        if (new_freq == 1) freq_one_count++;  // from >1 to 1
        entry->meta = META_ENCODE(new_freq, len);
        return new_freq;
    }
}


uint32_t seq_freq_set_existing(uint32_t idx, uint32_t freq, uint32_t node_id) {
    if (idx >= SEQ_MAP_CAPACITY || seqMap.entries[idx].sequence == NULL) {
        fprintf(stderr, "Invalid or empty index in seq_freq_set_existing\n");
        abort();
    }

    SeqFreqEntry *entry = &seqMap.entries[idx];
    uint32_t old_freq = META_GET_FREQ(entry->meta);
    uint8_t len = META_GET_LEN(entry->meta);

    entry->meta = META_ENCODE(freq, len);
    entry->node_id = node_id;

    // update freq_one_count accurately
    if (old_freq == 1 && freq != 1) freq_one_count--;
    else if (old_freq != 1 && freq == 1) freq_one_count++;

    return freq;
}
