#include "binseq_hashmap.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "xxhash.h"
#include "tree_node.h"

typedef struct {
    BinSeqKey key;
    BinSeqValue value;
    int used;
} Entry;

struct BinSeqMap {
    Entry *entries;
    size_t capacity;
    size_t size;
};

static inline uint64_t binseq_hash(BinSeqKey key) {
    if (key.binary_sequence == NULL || key.length == 0) {
        fprintf(stderr, "Error: Invalid key in binseq_hash\n");
        return 0;
    }
    return XXH3_64bits(key.binary_sequence, key.length);
}

static inline int binseq_equal(BinSeqKey a, BinSeqKey b) {
    if (a.length != b.length) return 0;
    if (a.length == 0) return 1;
    if (a.binary_sequence == NULL || b.binary_sequence == NULL) {
        fprintf(stderr, "Error: Null sequence in binseq_equal\n");
        return 0;
    }
    return memcmp(a.binary_sequence, b.binary_sequence, a.length) == 0;
}

static BinSeqKey copy_key(BinSeqKey key) {
    BinSeqKey new_key = {0};
    if (key.length > 0 && key.binary_sequence != NULL) {
        new_key.binary_sequence = malloc(key.length);
        if (new_key.binary_sequence == NULL) {
            fprintf(stderr, "Error: Failed to allocate memory in copy_key\n");
            new_key.length = 0;
            return new_key;
        }
        memcpy(new_key.binary_sequence, key.binary_sequence, key.length);
        new_key.length = key.length;
    }
    return new_key;
}

BinSeqMap *binseq_map_create(size_t capacity) {
    if (capacity == 0) {
        fprintf(stderr, "Error: Zero capacity in binseq_map_create\n");
        return NULL;
    }
    
    BinSeqMap *map = malloc(sizeof(BinSeqMap));
    if (map == NULL) {
        fprintf(stderr, "Error: Failed to allocate map structure\n");
        return NULL;
    }
    
    map->entries = calloc(capacity, sizeof(Entry));
    if (map->entries == NULL) {
        fprintf(stderr, "Error: Failed to allocate map entries\n");
        free(map);
        return NULL;
    }
    
    map->capacity = capacity;
    map->size = 0;
    return map;
}


void binseq_map_free(BinSeqMap *map) {
    if (!map) return;
    
    // Free all keys in the map
    for (size_t i = 0; i < map->capacity; i++) {
        if (map->entries[i].used) {
            free_key(map->entries[i].key);
        }
    }
    
    free(map->entries);
    free(map);
}

// Internal function to resize the map when it gets too full
static int binseq_map_resize(BinSeqMap *map, size_t new_capacity) {
    if (!map || new_capacity <= map->size) return 0;
    
    Entry *new_entries = calloc(new_capacity, sizeof(Entry));
    if (!new_entries) return 0;
    
    // Rehash all existing entries
    for (size_t i = 0; i < map->capacity; i++) {
        Entry *old_entry = &map->entries[i];
        if (!old_entry->used) continue;
        
        uint64_t hash = binseq_hash(old_entry->key);
        size_t index = hash % new_capacity;
        
        // Find empty slot in new table
        size_t try;
        for (try = 0; try < new_capacity; try++) {
            size_t pos = (index + try) % new_capacity;
            if (!new_entries[pos].used) {
                new_entries[pos] = *old_entry;
                break;
            }
        }
        
        if (try == new_capacity) {
            free(new_entries);
            return 0;  // Resize failed
        }
    }
    
    free(map->entries);
    map->entries = new_entries;
    map->capacity = new_capacity;
    return 1;
}

int binseq_map_put(BinSeqMap *map, BinSeqKey key, BinSeqValue value) {
    if (!map || !key.binary_sequence || key.length == 0) return 0;
    
    // Resize if load factor > 0.7
    if (map->size * 10 > map->capacity * 7) {
        if (!binseq_map_resize(map, map->capacity * 2)) {
            fprintf(stderr, "Warning: map resize failed\n");
            // Continue anyway, but performance will degrade
        }
    }
    
    uint64_t hash = binseq_hash(key);
    size_t index = hash % map->capacity;
    
    for (size_t i = 0; i < map->capacity; i++) {
        size_t try = (index + i) % map->capacity;
        Entry *entry = &map->entries[try];
        
        if (!entry->used) {
            // Create a copy of the key
            BinSeqKey key_copy = copy_key(key);
            if (!key_copy.binary_sequence) return 0;
            
            entry->key = key_copy;
            entry->value = value;
            entry->used = 1;
            map->size++;
            return 1;
        }
        
        if (binseq_equal(entry->key, key)) {
            // Update existing entry
            entry->value = value;
            return 1;
        }
    }
    
    return 0;  // Shouldn't happen if resize works
}

BinSeqValue *binseq_map_get(BinSeqMap *map, BinSeqKey key) {
    if (!map || !key.binary_sequence || key.length == 0) return NULL;
    
    uint64_t hash = binseq_hash(key);
    size_t index = hash % map->capacity;
    
    for (size_t i = 0; i < map->capacity; i++) {
        size_t try = (index + i) % map->capacity;
        Entry *entry = &map->entries[try];
        
        if (!entry->used) break;  // Not found
        if (binseq_equal(entry->key, key)) {
            return &entry->value;
        }
    }
    
    return NULL;
}

int binseq_map_contains(BinSeqMap *map, BinSeqKey key) {
    return binseq_map_get(map, key) != NULL;
}

void copyMap(struct TreeNode* mapSource, struct TreeNode* mapTarget, int increaseSize) {
    if (!mapSource || !mapTarget || !mapSource->map) return;
    
    BinSeqMap *source = mapSource->map;
    size_t new_capacity = source->capacity + (increaseSize > 0 ? increaseSize : 0);
    
    BinSeqMap *target = binseq_map_create(new_capacity);
    if (!target) {
        fprintf(stderr, "Failed to create target map in copyMap\n");
        return;
    }
    
    for (size_t i = 0; i < source->capacity; i++) {
        Entry *entry = &source->entries[i];
        if (entry->used) {
            BinSeqKey key_copy = copy_key(entry->key);
            if (!key_copy.binary_sequence) continue;
            
            if (!binseq_map_put(target, key_copy, entry->value)) {
                free_key(key_copy);
                fprintf(stderr, "Failed to insert entry during copyMap\n");
            }
        }
    }
    
    // Free old map if exists
    if (mapTarget->map) {
        binseq_map_free(mapTarget->map);
    }
    mapTarget->map = target;
}

BinSeqKey create_binseq_key(const uint8_t* data, uint16_t length) {
    BinSeqKey key = {0};
    
    if (!data || length == 0 || length > COMPRESS_SEQUENCE_LENGTH) {
    	fprintf(stderr, "create_binseq_key has invalid parameters\n");
        key.length = 0; // Mark as invalid
        return key;
    }

    key.length = length;
    key.binary_sequence = malloc(length);
    if (!key.binary_sequence) {
        	fprintf(stderr, "create_binseq_key unable to malloc key\n");
            key.length = 0; // Mark as invalid if allocation fails
            return key;
    }
    memcpy(key.binary_sequence, data, length);
    return key;
}


void free_key(BinSeqKey key) {
    if (key.binary_sequence) {
        free(key.binary_sequence);
    } else {
        fprintf(stderr, "Warning: Attempted to free NULL binary_sequence\n");
    }
}

