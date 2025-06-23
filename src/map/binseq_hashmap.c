//binseq_hashmap.c

#include "binseq_hashmap.h"
#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include <assert.h>

#define EVICTION_LOOKBACK 4

SequenceRepository sequence_repo;
static inline void verify_lru(const BinSeqMap* map);

// Initialize the sequence repository (call this once at program start)
void binseq_map_global_init() {
    seq_repo_init(&sequence_repo);
}

// Cleanup the sequence repository (call this at program end)
void binseq_map_global_cleanup() {
    seq_repo_cleanup(&sequence_repo);
}

static inline bool sequences_equal(uint32_t seq_id_a, uint32_t seq_id_b) {
    if (seq_id_a == seq_id_b) return true;
    
    const uint8_t* a = seq_repo_get_data(&sequence_repo, seq_id_a);
    const uint8_t* b = seq_repo_get_data(&sequence_repo, seq_id_b);
    uint16_t a_len = seq_repo_get_length(&sequence_repo, seq_id_a);
    uint16_t b_len = seq_repo_get_length(&sequence_repo, seq_id_b);
    
    return a_len == b_len && memcmp(a, b, a_len) == 0;
}

void binseq_map_init(BinSeqMap* map) {
    for (uint16_t i = 0; i < HASH_MAP_SIZE; i++) {
        map->entries[i].next = i + 1;
        map->entries[i].used = 0;
        map->entries[i].prev = UNUSED_INDEX;
    }
    map->entries[HASH_MAP_SIZE-1].next = UNUSED_INDEX;
    map->free_head = 0;
    map->total_used = 0;
    map->lru_head = UNUSED_INDEX;
    map->lru_tail = UNUSED_INDEX;
    map->size = 0;
}

static inline void lru_remove(BinSeqMap* map, uint16_t index) {
    Entry* e = &map->entries[index];

    if (!e->used) return;

    if (e->prev != UNUSED_INDEX) {
        map->entries[e->prev].next = e->next;
    } else {
        map->lru_head = e->next;
    }

    if (e->next != UNUSED_INDEX) {
        map->entries[e->next].prev = e->prev;
    } else {
        map->lru_tail = e->prev;
    }

    e->next = UNUSED_INDEX;
    e->prev = UNUSED_INDEX;
    map->size--;
    verify_lru(map);
}

static inline void lru_push_front(BinSeqMap *map, uint16_t index) {
    Entry *e = &map->entries[index];

    if (e->used && (e->prev != UNUSED_INDEX || e->next != UNUSED_INDEX || 
                   map->lru_head == index || map->lru_tail == index)) {
        lru_remove(map, index);
    }

    e->next = map->lru_head;
    e->prev = UNUSED_INDEX;

    if (map->lru_head != UNUSED_INDEX) {
        map->entries[map->lru_head].prev = index;
    } else {
        map->lru_tail = index;
    }

    map->lru_head = index;
    
    if (!e->used) {
        map->total_used++;
        e->used = 1;
    }
    
    map->size++;
    verify_lru(map);
}

static inline int calculate_savings(uint16_t length, uint32_t frequency) {
    return (length - 1) * (length - 1) * (frequency - 1);
}

static uint16_t get_free_slot(BinSeqMap* map, uint16_t hash_index) {
    for (int i = 0; i < EVICTION_LOOKBACK; i++) {
        uint16_t slot = (hash_index + i) % HASH_MAP_SIZE;
        if (!map->entries[slot].used) {
            return slot;
        }
    }

    if (map->lru_tail == UNUSED_INDEX) {
        return UNUSED_INDEX;
    }

    uint16_t candidate = map->lru_tail;
    Entry* e = &map->entries[candidate];
    
    e->sequence_id = 0;
    e->frequency = 0;
    e->cached_hash = 0;
    e->used = 1;
    
    lru_remove(map, candidate);
    return candidate;
}

static void insert_entry(BinSeqMap* map, uint16_t slot,
                       const uint8_t* key_sequence, uint16_t key_length,
                       int value_frequency, uint32_t current_level,
                       uint64_t hash) {
    Entry* e = &map->entries[slot];
    
    e->sequence_id = seq_repo_add(&sequence_repo, key_sequence, key_length);
    e->frequency = value_frequency;
    e->last_updated_level = current_level;
    e->cached_hash = hash;
    
    lru_push_front(map, slot);
}

static int binseq_map_insert_fresh(BinSeqMap* map, const uint8_t* key_sequence,
                  uint16_t key_length, int value_frequency,
                  uint32_t current_level) {
    uint64_t hash = XXH3_64bits(key_sequence, key_length);
    uint16_t index = hash % HASH_MAP_SIZE;
       
    uint16_t slot = get_free_slot(map, index);
    if (slot == UNUSED_INDEX) return 0;
    
    insert_entry(map, slot, key_sequence, key_length, 
                value_frequency, current_level, hash);
    return 1;
}

int binseq_map_put(BinSeqMap* map, const uint8_t* key_sequence,
                  uint16_t key_length, int value_frequency,
                  uint32_t current_level) {
    uint64_t hash = XXH3_64bits(key_sequence, key_length);
    uint16_t index = hash % HASH_MAP_SIZE;
    
    for (int i = 0; i < 4; i++) {
        uint16_t slot = (index + i) % HASH_MAP_SIZE;
        Entry* e = &map->entries[slot];
        
        if (e->used && e->cached_hash == hash) {
            const uint8_t* seq_data = seq_repo_get_data(&sequence_repo, e->sequence_id);
            uint16_t seq_len = seq_repo_get_length(&sequence_repo, e->sequence_id);
            
            if (seq_len == key_length && memcmp(seq_data, key_sequence, key_length) == 0) {
                e->frequency = value_frequency;
                e->last_updated_level = current_level;
                lru_remove(map, slot);
                e->cached_hash = hash;
                lru_push_front(map, slot);
                return 1;
            }
        }
    }
    
    uint16_t slot = get_free_slot(map, index);
    if (slot == UNUSED_INDEX) return 0;
    
    insert_entry(map, slot, key_sequence, key_length,
                value_frequency, current_level, hash);
    return 1;
}

static inline void verify_lru(const BinSeqMap* map) {
    uint16_t lru_count = 0;
    uint16_t used_count = 0;
    uint16_t current = map->lru_head;
    
    while (current != UNUSED_INDEX && lru_count <= HASH_MAP_SIZE) {
        assert(map->entries[current].used);
        lru_count++;
        current = map->entries[current].next;
    }
    
    for (int i = 0; i < HASH_MAP_SIZE; i++) {
        if (map->entries[i].used) used_count++;
    }
    
    if (lru_count != map->size) {
        fprintf(stderr, "LRU count mismatch: actual=%u, expected=%zu\n",
                lru_count, map->size);
        abort();
    }
    
    if (used_count != map->total_used) {
        fprintf(stderr, "Used count mismatch: actual=%u, expected=%u\n",
                used_count, map->total_used);
        abort();
    }

    if (map->size > 0) {
        assert(map->entries[map->lru_tail].next == UNUSED_INDEX);
    }
}

static const Entry* binseq_map_full_lookup(const BinSeqMap* map,
                                         const uint8_t* key_sequence,
                                         uint16_t key_length) {
    uint64_t hash = XXH3_64bits(key_sequence, key_length);
    uint16_t index = hash % HASH_MAP_SIZE;

    for (int i = 0; i < HASH_MAP_SIZE; i++) {
        uint16_t slot = (index + i) % HASH_MAP_SIZE;
        const Entry* e = &map->entries[slot];

        if (!e->used) continue;

        if (e->cached_hash == hash) {
            const uint8_t* seq_data = seq_repo_get_data(&sequence_repo, e->sequence_id);
            uint16_t seq_len = seq_repo_get_length(&sequence_repo, e->sequence_id);
            
            if (seq_len == key_length && memcmp(seq_data, key_sequence, key_length) == 0) {
                return e;
            }
        }
    }

    return NULL;
}

int binseq_map_increment_frequency(BinSeqMap* map,
                                 const uint8_t* key_sequence,
                                 uint16_t key_length,
                                 uint32_t current_level, uint32_t *total_savings) {
    const Entry* existing = binseq_map_fast_lookup(map, key_sequence, key_length);
    if (!existing) {
        existing = binseq_map_full_lookup(map, key_sequence, key_length);
    }

    if (existing) {
        Entry* e = (Entry*)existing;
        e->frequency++;
        e->last_updated_level = current_level;
        uint16_t seq_len = seq_repo_get_length(&sequence_repo, e->sequence_id);
        *total_savings += calculate_savings(seq_len, e->frequency);
        uint16_t index = e - map->entries;
        lru_remove(map, index);
        lru_push_front(map, index);
        return 1;
    }

    return binseq_map_insert_fresh(map, key_sequence, key_length, 1, current_level);
}

const Entry *binseq_map_fast_lookup(const BinSeqMap *map,
                                  const uint8_t *key_sequence,
                                  uint16_t key_length) {
    uint64_t hash = XXH3_64bits(key_sequence, key_length);
    uint16_t index = hash % HASH_MAP_SIZE;

    for (int i = 0; i < 2; i++) {
        uint16_t slot = (index + i) % HASH_MAP_SIZE;
        const Entry *e = &map->entries[slot];
        if (e->used && e->cached_hash == hash) {
            const uint8_t* seq_data = seq_repo_get_data(&sequence_repo, e->sequence_id);
            uint16_t seq_len = seq_repo_get_length(&sequence_repo, e->sequence_id);
            
            if (seq_len == key_length && memcmp(seq_data, key_sequence, key_length) == 0) {
                return e;
            }
        }
    }
    return NULL;
}

void binseq_map_reset(BinSeqMap* map) {
    map->free_head = 0;
    for (uint16_t i = 0; i < HASH_MAP_SIZE; i++) {
        map->entries[i].next = i + 1;
        map->entries[i].used = 0;
        map->entries[i].prev = UNUSED_INDEX;
    }
    map->entries[HASH_MAP_SIZE-1].next = UNUSED_INDEX;
    
    map->lru_head = UNUSED_INDEX;
    map->lru_tail = UNUSED_INDEX;
    map->size = 0;
    map->total_used = 0;
}

void print_hashmap(const BinSeqMap* map) {
    verify_lru(map);
    printf("Hashmap (Size: %zu/%d)\n", map->size, HASH_MAP_SIZE);
    printf("LRU Order: ");
    
    uint16_t current = map->lru_head;
    while (current != UNUSED_INDEX) {
        printf("%d->", current);
        current = map->entries[current].next;
    }
    printf("NULL\n");
    
    for (int i = 0; i < HASH_MAP_SIZE; i++) {
        const Entry* e = &map->entries[i];
        if (!e->used) continue;
        
        const uint8_t* seq_data = seq_repo_get_data(&sequence_repo, e->sequence_id);
        uint16_t seq_len = seq_repo_get_length(&sequence_repo, e->sequence_id);
        
        printf("[%d] Len:%d Freq:%d Lvl:%d Seq:", i, seq_len, e->frequency, e->last_updated_level);
        for (int j = 0; j < seq_len; j++)
            printf("%02X ", seq_data[j]);
        printf("\n");
    }
}