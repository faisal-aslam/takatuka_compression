//binseq_hashmap.c

#include "binseq_hashmap.h"
#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include <assert.h>

#define EVICTION_LOOKBACK 4

static inline void verify_lru(const BinSeqMap* map);

void binseq_map_init(BinSeqMap* map) {
    // Initialize free list
    for (uint16_t i = 0; i < HASH_MAP_SIZE; i++) {
        map->entries[i].next = i + 1;
        map->entries[i].used = 0;
        map->entries[i].prev = UNUSED_INDEX;
    }
    map->entries[HASH_MAP_SIZE-1].next = UNUSED_INDEX;
    map->free_head = 0;
    map->total_used = 0;
    
    // Initialize LRU list
    map->lru_head = UNUSED_INDEX;
    map->lru_tail = UNUSED_INDEX;
    map->size = 0;
}

static inline void lru_remove(BinSeqMap* map, uint16_t index) {
    Entry* e = &map->entries[index];

    // Update previous node's next pointer
    if (e->prev != UNUSED_INDEX) {
        map->entries[e->prev].next = e->next;
    } else {
        // Removing the head
        map->lru_head = e->next;
    }

    // Update next node's prev pointer
    if (e->next != UNUSED_INDEX) {
        map->entries[e->next].prev = e->prev;
    } else {
        // Removing the tail
        map->lru_tail = e->prev;
    }

    // Clear removed entry's LRU links
    e->next = UNUSED_INDEX;
    e->prev = UNUSED_INDEX;
    map->size--;
    verify_lru(map);
}

static inline void lru_push_front(BinSeqMap *map, uint16_t index) {
    Entry *e = &map->entries[index];
    
    // Clear existing links
    e->next = UNUSED_INDEX;
    e->prev = UNUSED_INDEX;

    if (map->lru_head == UNUSED_INDEX) {
        // First entry
        map->lru_head = index;
        map->lru_tail = index;
    } else {
        // Normal insertion
        e->next = map->lru_head;
        map->entries[map->lru_head].prev = index;
        map->lru_head = index;
    }
    e->used = 1;
    verify_lru(map);
}

static inline int calculate_savings(const Entry* e) {
    return (e->length - 1) * (e->frequency - 1);
}

static uint16_t get_free_slot(BinSeqMap* map, uint16_t hash_index) {
    // First try to find empty slot near hash index
    for (int i = 0; i < EVICTION_LOOKBACK; i++) {
        uint16_t slot = (hash_index + i) % HASH_MAP_SIZE;
        if (!map->entries[slot].used) {
            return slot;
        }
    }

    // If no empty slots, evict LRU entry
    if (map->lru_tail == UNUSED_INDEX) {
        return UNUSED_INDEX;
    }

    uint16_t candidate = map->lru_tail;
    lru_remove(map, candidate);
    
    // Clear the evicted entry
    Entry* e = &map->entries[candidate];
    e->used = 0;
    e->binary_sequence = NULL;
    e->length = 0;
    e->frequency = 0;
    
    return candidate;
}

static inline bool sequences_equal(const uint8_t* a, uint16_t a_len,
                                 const uint8_t* b, uint16_t b_len) {
    return a_len == b_len && memcmp(a, b, a_len) == 0;
}

static void insert_entry(BinSeqMap* map, uint16_t slot,
                       const uint8_t* key_sequence, uint16_t key_length,
                       int value_frequency, uint32_t current_level,
                       uint64_t hash) {
    Entry* e = &map->entries[slot];
    
    e->binary_sequence = (uint8_t*)key_sequence;
    e->length = key_length;
    e->frequency = value_frequency;
    e->last_updated_level = current_level;
    e->cached_hash = hash;
    
    map->total_used++;
    map->size++;
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
    
    // Check first 4 slots for existing entry
    for (int i = 0; i < 4; i++) {
        uint16_t slot = (index + i) % HASH_MAP_SIZE;
        Entry* e = &map->entries[slot];
        
        if (e->used && sequences_equal(e->binary_sequence, e->length, 
                                      key_sequence, key_length)) {
            e->frequency = value_frequency;
            e->last_updated_level = current_level;
            lru_remove(map, slot);
            e->cached_hash = hash;
            lru_push_front(map, slot);
            return 1;
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
    
    // Count LRU entries
    uint16_t current = map->lru_head;
    while (current != UNUSED_INDEX && lru_count <= HASH_MAP_SIZE) {
        assert(map->entries[current].used);
        lru_count++;
        current = map->entries[current].next;
    }
    
    // Count used entries
    for (int i = 0; i < HASH_MAP_SIZE; i++) {
        if (map->entries[i].used) used_count++;
    }
    
    assert(lru_count == map->size);
    assert(used_count == map->total_used);
    assert(map->total_used >= map->size);
    
    // Verify tail points to last element
    if (map->size > 0) {
        current = map->lru_tail;
        assert(map->entries[current].next == UNUSED_INDEX);
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

        if (!e->used) return NULL;

        if (e->cached_hash == hash &&
            sequences_equal(e->binary_sequence, e->length, key_sequence, key_length)) {
            return e;
        }
    }

    return NULL;
}

int binseq_map_increment_frequency(BinSeqMap* map,
                                 const uint8_t* key_sequence,
                                 uint16_t key_length,
                                 uint32_t current_level) {
    const Entry* existing = binseq_map_fast_lookup(map, key_sequence, key_length);
    if (!existing) {
        existing = binseq_map_full_lookup(map, key_sequence, key_length);
    }

    if (existing) {
        Entry* e = (Entry*)existing;
        e->frequency++;
        e->last_updated_level = current_level;
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
        if (e->used && e->cached_hash == hash &&
            sequences_equal(e->binary_sequence, e->length, key_sequence, key_length)) {
            return e;
        }
    }
    return NULL;
}

void binseq_map_reset(BinSeqMap* map) {
    // Rebuild free list
    map->free_head = 0;
    for (uint16_t i = 0; i < HASH_MAP_SIZE; i++) {
        map->entries[i].next = i + 1;
        map->entries[i].used = 0;
        map->entries[i].prev = UNUSED_INDEX;
    }
    map->entries[HASH_MAP_SIZE-1].next = UNUSED_INDEX;
    
    // Reset LRU
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
        
        printf("[%d] Len:%d Freq:%d Lvl:%d Seq:",
              i, e->length, e->frequency, e->last_updated_level);
        for (int j = 0; j < e->length; j++)
            printf("%02X ", e->binary_sequence[j]);
        printf("\n");
    }
}