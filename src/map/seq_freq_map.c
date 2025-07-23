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

typedef struct {
    const uint8_t *sequence;  // 8 bytes: external pointer
    uint32_t meta;            // 4 bytes: upper 8 bits = length, lower 24 bits = frequency
} SeqFreqEntry;

typedef struct SeqFreqMap {
    SeqFreqEntry entries[SEQ_MAP_CAPACITY];
} SeqFreqMap;

SeqFreqMap seqMap;

void init_seq_freq_map(void) {
    memset(&seqMap, 0, sizeof(SeqFreqMap));
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

uint32_t seq_freq_increment(const uint8_t *seq, uint8_t len) {
    uint64_t hash = XXH3_64bits_withSeed(seq, len, 0);
    int found;
    uint32_t idx = find_slot(seq, len, hash, &found);

    SeqFreqEntry *entry = &seqMap.entries[idx];

    if (found) {
        uint32_t freq = META_GET_FREQ(entry->meta) + 1;
        entry->meta = META_ENCODE(freq, len);
        return freq;
    } else {
        entry->sequence = seq;
        entry->meta = META_ENCODE(1, len);
        return 1;
    }
}

uint32_t seq_freq_set(const uint8_t *seq, uint8_t len, uint32_t freq) {
    if (freq > 0xFFFFFF) {
        fprintf(stderr, "Frequency exceeds 24-bit limit\n");
        abort();
    }

    uint64_t hash = XXH3_64bits_withSeed(seq, len, 0);
    int found;
    uint32_t idx = find_slot(seq, len, hash, &found);

    SeqFreqEntry *entry = &seqMap.entries[idx];
    entry->sequence = seq;
    entry->meta = META_ENCODE(freq, len);
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
    uint32_t freq = META_GET_FREQ(entry->meta);

    if (freq == 0) {
        fprintf(stderr, "Invalid decrement — frequency already 0\n");
        abort();
    }

    freq--;

    if (freq == 0) {
        entry->sequence = NULL;
        entry->meta = 0;
        return 0;
    } else {
        entry->meta = META_ENCODE(freq, len);
        return freq;
    }
}

uint32_t seq_freq_get(const uint8_t *seq, uint8_t len) {
    uint64_t hash = XXH3_64bits_withSeed(seq, len, 0);
    int found;
    uint32_t idx = find_slot(seq, len, hash, &found);

    return found ? META_GET_FREQ(seqMap.entries[idx].meta) : 0;
}
