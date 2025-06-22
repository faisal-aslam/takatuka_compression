//binseq_hashmap.c

#include "binseq_hashmap.h"
#include <string.h>
#include <limits.h>
#include <stdbool.h>

#define EVICTION_LOOKBACK 4

void binseq_map_init(BinSeqMap* map) {
    // Initialize free list
    for (uint16_t i = 0; i < HASH_MAP_SIZE; i++) {
        map->entries[i].next = i + 1;
        map->entries[i].used = 0;
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
}


static inline void lru_push_front(BinSeqMap *map, uint16_t index) {
    Entry *e = &map->entries[index];
    e->next = map->lru_head;
    e->prev = UNUSED_INDEX;

    if (map->lru_head != UNUSED_INDEX) {
        map->entries[map->lru_head].prev = index;
    } else {
        map->lru_tail = index;  // This is the only node, so also tail
    }

    map->lru_head = index;
}


static inline int calculate_savings(const Entry* e) {
    return (e->length - 1) * (e->frequency - 1);
}

static uint16_t get_free_slot(BinSeqMap* map) {
    if (map->free_head != UNUSED_INDEX) {
        uint16_t slot = map->free_head;
        map->free_head = map->entries[slot].next;
        map->total_used++;  //track used
        return slot;
    }
    
    // Evict LRU entry with worst savings
    uint16_t candidate = map->lru_tail;
    int min_savings = calculate_savings(&map->entries[candidate]);
    
    // Check last EVICTION_LOOKBACK entries for better candidates
    uint16_t current = map->entries[candidate].prev;
    for (int i = 0; i < EVICTION_LOOKBACK && current != UNUSED_INDEX; i++) {
        int savings = calculate_savings(&map->entries[current]);
        if (savings < min_savings) {
            candidate = current;
            min_savings = savings;
        }
        current = map->entries[current].prev;
    }
    
    lru_remove(map, candidate);
    // Sanity: evicted entries must not be in the free list
    map->entries[candidate].used = 0;
    map->entries[candidate].binary_sequence = NULL;  // Clear pointer
    map->entries[candidate].length = 0;
    map->entries[candidate].frequency = 0;
    return candidate;
}

static inline bool sequences_equal(const uint8_t* a, uint16_t a_len,
                                 const uint8_t* b, uint16_t b_len) {
    return a_len == b_len && memcmp(a, b, a_len) == 0;
}

static int binseq_map_insert_fresh(BinSeqMap* map, const uint8_t* key_sequence,
                  uint16_t key_length, int value_frequency,
                  uint32_t current_level) {
    uint64_t hash = XXH3_64bits(key_sequence, key_length);
    
    uint16_t index = hash % HASH_MAP_SIZE;
       
    // Insert new entry
    uint16_t slot = get_free_slot(map);
    Entry* e = &map->entries[slot];
    
    e->binary_sequence = (uint8_t*)key_sequence;
    e->length = key_length;
    e->frequency = value_frequency;
    e->last_updated_level = current_level;
    e->cached_hash = hash; 
    e->used = 1;
    lru_push_front(map, slot);
    map->size++;
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
            e->cached_hash = hash;  // Store for faster rehashing    
            lru_push_front(map, slot);
            return 1;
        }
    }
    
    // Insert new entry
    uint16_t slot = get_free_slot(map);
    Entry* e = &map->entries[slot];
    
    e->binary_sequence = (uint8_t*)key_sequence;
    e->length = key_length;
    e->frequency = value_frequency;
    e->last_updated_level = current_level;
    e->cached_hash = hash; 
    e->used = 1;
    lru_push_front(map, slot);
    map->size++;
    return 1;
}

static const Entry* binseq_map_full_lookup(const BinSeqMap* map,
                                           const uint8_t* key_sequence,
                                           uint16_t key_length,
                                           uint64_t hash) {
    uint16_t index = hash % HASH_MAP_SIZE;

    for (int i = 0; i < HASH_MAP_SIZE; i++) {
        uint16_t slot = (index + i) % HASH_MAP_SIZE;
        const Entry* e = &map->entries[slot];

        if (!e->used) {
            // First empty slot: key cannot be found beyond this point
            return NULL;
        }

        if (e->cached_hash == hash &&
            sequences_equal(e->binary_sequence, e->length, key_sequence, key_length)) {
            return e;
        }
    }

    // This should be unreachable if load factor < 1
    return NULL;
}


int binseq_map_increment_frequency(BinSeqMap* map,
                                 const uint8_t* key_sequence,
                                 uint16_t key_length,
                                 uint32_t current_level) {
    uint64_t hash = XXH3_64bits(key_sequence, key_length);

    // Try fast path (2-slot probe)
    const Entry* existing = binseq_map_fast_lookup(map, key_sequence, key_length);
    
    if (!existing) { //not found.
        // Fallback to full probe
        existing = binseq_map_full_lookup(map, key_sequence, key_length, hash);
    }

    if (existing) { //found
        Entry* e = (Entry*)existing;
        e->frequency++;
        e->last_updated_level = current_level;
        uint16_t index = e - map->entries;
        lru_remove(map, index);
        lru_push_front(map, index);
        return 1;
    }

    // Still not found — insert fresh
    return binseq_map_insert_fresh(map, key_sequence, key_length, 1, current_level);
}

const Entry *binseq_map_fast_lookup(const BinSeqMap *map,
                                    const uint8_t *key_sequence,
                                    uint16_t key_length) {
    uint64_t hash = XXH3_64bits(key_sequence, key_length);
    uint16_t index = hash % HASH_MAP_SIZE;

    // Check 2 slots only (constant time)
    for (int i = 0; i < 2; i++) {
        // Prefetch next entry while checking current one
        __builtin_prefetch(&map->entries[(index + i + 1) % HASH_MAP_SIZE]);
        const Entry *e = &map->entries[(index + i) % HASH_MAP_SIZE];
        if (e->used && e->cached_hash == hash &&
            sequences_equal(e->binary_sequence, e->length, key_sequence,
                            key_length)) {
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
    }
    map->entries[HASH_MAP_SIZE-1].next = UNUSED_INDEX;
    
    // Reset LRU
    map->lru_head = UNUSED_INDEX;
    map->lru_tail = UNUSED_INDEX;
    map->size = 0;
    map->total_used = 0;//reset use to 0
}

void print_hashmap(const BinSeqMap* map) {
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