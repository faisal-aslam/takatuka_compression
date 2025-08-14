// sp_sf_map.c

#include "sp_seq_freq_map.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "general_map.h"

#define EMPTY_HASH 0ULL
#define TOMBSTONE_HASH 1ULL  // Special marker for deleted slots
#define HASH_SEED 0x9E3779B185EBCA87ULL

typedef struct {
    uint64_t hash;
    SFEntry entry;
} SFSlot;

static SFSlot *sf_slots = NULL;
static uint16_t sf_capacity = 0;
static uint16_t sf_count = 0;

static inline uint64_t hash_sequence(const uint8_t *seq, uint8_t len) {
    return XXH3_64bits_withSeed(seq, len, HASH_SEED);
}

void init_sf_map(uint16_t capacity) {
    // Require power-of-two capacity for fast masking
    if ((capacity & (capacity - 1)) != 0) {
        fprintf(stderr, "Capacity must be power of two\n");
        exit(EXIT_FAILURE);
    }

    sf_capacity = capacity;
    sf_count = 0;
    sf_slots = (SFSlot *)calloc(capacity, sizeof(SFSlot));
    if (!sf_slots) {
        fprintf(stderr, "Failed to allocate seq freq map\n");
        exit(EXIT_FAILURE);
    }
}



static SFSlot *find_slot(const uint8_t *seq, uint8_t len, uint64_t hash, bool *found) {
    uint16_t mask = sf_capacity - 1;
    uint16_t idx = (uint16_t)(hash & mask);
    uint16_t first_tombstone = UINT16_MAX;

    for (;;) {
        SFSlot *slot = &sf_slots[idx];

        if (slot->hash == EMPTY_HASH) {
            // Return first tombstone if found, otherwise empty
            *found = false;
            return (first_tombstone != UINT16_MAX) ? &sf_slots[first_tombstone] : slot;
        }
        if (slot->hash == TOMBSTONE_HASH) {
            if (first_tombstone == UINT16_MAX)
                first_tombstone = idx;
        } else if (slot->hash == hash &&
                   slot->entry.length == len &&
                   sequences_equal(slot->entry.sequence, seq, len)) {
            *found = true;
            return slot;
        }

        idx = (idx + 1) & mask;
    }
}

uint32_t sf_increment(const uint8_t *seq, uint8_t len) {
    uint64_t hash = hash_sequence(seq, len);
    bool found;
    SFSlot *slot = find_slot(seq, len, hash, &found);

    if (found) {
        return ++slot->entry.freq;
    }

    slot->hash = hash;
    slot->entry.sequence = seq;
    slot->entry.length = len;
    slot->entry.freq = 1;
    sf_count++;
    return 1;
}

uint32_t sf_decrement(const uint8_t *seq, uint8_t len) {
    uint64_t hash = hash_sequence(seq, len);
    bool found;
    SFSlot *slot = find_slot(seq, len, hash, &found);

    if (!found || slot->entry.freq == 0)
        return 0;

    uint32_t new_freq = --slot->entry.freq;
    if (new_freq == 0) {
        slot->hash = TOMBSTONE_HASH; // Mark deleted
        sf_count--;
    }
    return new_freq;
}

uint32_t sf_set(const uint8_t *seq, uint8_t len, uint32_t freq) {
    uint64_t hash = hash_sequence(seq, len);
    bool found;
    SFSlot *slot = find_slot(seq, len, hash, &found);

    if (!found) {
        slot->hash = hash;
        slot->entry.sequence = seq;
        slot->entry.length = len;
    }
    slot->entry.freq = freq;

    if (freq == 0) {
        slot->hash = TOMBSTONE_HASH; // Treat as deletion
        sf_count--;
    } else if (!found) {
        sf_count++;
    }
    return freq;
}

uint32_t sf_get(const uint8_t *seq, uint8_t len) {
    uint64_t hash = hash_sequence(seq, len);
    bool found;
    SFSlot *slot = find_slot(seq, len, hash, &found);
    return found ? slot->entry.freq : 0;
}

void sf_map_print() {
    printf("---- SFMap Contents ----\n");
    for (uint16_t i = 0; i < sf_capacity; i++) {
        if (sf_slots[i].hash != EMPTY_HASH &&
            sf_slots[i].hash != TOMBSTONE_HASH &&
            sf_slots[i].entry.freq > 0) {

            printf("SeqLen=%u Freq=%u, seq=",
                   sf_slots[i].entry.length,
                   sf_slots[i].entry.freq);

            for (uint8_t j = 0; j < sf_slots[i].entry.length; j++) {
                uint8_t c = sf_slots[i].entry.sequence[j];
                if (c >= 32 && c <= 126)
                    putchar(c);
                else
                    printf("\\x%02X", c); // Print hex for non-printables
            }
            putchar('\n');
        }
    }
    printf("-----------------------------\n");
}
