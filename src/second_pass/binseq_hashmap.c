// ----> binseq_hashmap.c 
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

size_t get_map_size(BinSeqMap* map) {
	if (!map) {
        fprintf(stderr, "Error: Map is NULL\n");
		return 0;
	}
	return map->size;
}

size_t get_map_capacity(BinSeqMap* map) {
	if (!map) {
        fprintf(stderr, "Error: Map is NULL\n");
		return 0;
	}
	return map->capacity;
}


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
    
    // Check for NULL entries
    if (!map->entries) {
        fprintf(stderr, "Warning: Attempted to free map with NULL entries\n");
        free(map);
        return;
    }

    // Free all keys and values
    for (size_t i = 0; i < map->capacity; i++) {
        if (map->entries[i].used) {
            free_key(map->entries[i].key);
            free_binseq_value(map->entries[i].value);
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
    if (!map || !key.binary_sequence || key.length == 0) {
        free_key(key);
        free_binseq_value(value);  // Free the value's location array
        return 0;
    }
    
    // Resize if needed
    if (map->size * 10 > map->capacity * 7) {
        if (!binseq_map_resize(map, map->capacity * 2)) {
            free_key(key);
            free_binseq_value(value);
            return 0;
        }
    }
    
    uint64_t hash = binseq_hash(key);
    size_t index = hash % map->capacity;
    
    for (size_t i = 0; i < map->capacity; i++) {
        size_t try = (index + i) % map->capacity;
        Entry *entry = &map->entries[try];
        
        if (!entry->used) {
            // Take ownership of the key
            entry->key = key;
            
            // Take ownership of the value's location array
            entry->value.frequency = value.frequency;
            entry->value.seqLocation = value.seqLocation;
            entry->value.seqLocationLength = value.seqLocationLength;
            entry->value.seqLocationCapacity = value.seqLocationCapacity;
            
            // Zero out the source value to prevent double-free
            value.seqLocation = NULL;
            value.seqLocationLength = 0;
            value.seqLocationCapacity = 0;
            
            entry->used = 1;
            map->size++;
            return 1;
        }
        
        if (binseq_equal(entry->key, key)) {
            // Free the passed key since we won't use it
            free_key(key);
            
            // Free the old value's location array
            free_binseq_value(entry->value);
            
            // Take ownership of the new value's location array
            entry->value.frequency = value.frequency;
            entry->value.seqLocation = value.seqLocation;
            entry->value.seqLocationLength = value.seqLocationLength;
            entry->value.seqLocationCapacity = value.seqLocationCapacity;
            
            // Zero out the source value
            value.seqLocation = NULL;
            value.seqLocationLength = 0;
            value.seqLocationCapacity = 0;
            
            return 1;
        }
    }
    
    // If we get here, the map is full
    free_key(key);
    free_binseq_value(value);
    return 0;
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

void copyMap(struct TreeNode* mapSource, struct TreeNode* mapTarget) {
    if (!mapSource || !mapTarget) {
        fprintf(stderr, "copyMap: Invalid parameters\n");
        return;
    }

    // Handle self-copy
    if (mapTarget == mapSource) return;

    // If source has no map, clear target
    if (!mapSource->map) {
        if (mapTarget->map) {
            binseq_map_free(mapTarget->map);
            mapTarget->map = NULL;
        }
        return;
    }

    // Create new map
    BinSeqMap *target = binseq_map_create(mapSource->map->capacity + 1);
    if (!target) return;

    // Deep copy entries
    for (size_t i = 0; i < mapSource->map->capacity; i++) {
        Entry *src = &mapSource->map->entries[i];
        if (src->used) {
            BinSeqKey key = copy_key(src->key);
            BinSeqValue val = create_binseq_value(src->value.frequency, 
                                                src->value.seqLocation,
                                                src->value.seqLocationLength);
            if (!binseq_map_put(target, key, val)) {
                free_key(key);
                free_binseq_value(val);
            }
        }
    }

    // Free old map if different
    if (mapTarget->map && mapTarget->map != mapSource->map) {
        binseq_map_free(mapTarget->map);
    }
    mapTarget->map = target;
}

BinSeqKey create_binseq_key(const uint8_t* sequence, uint16_t length) {
    BinSeqKey key = {0};
    
    if (!sequence || length == 0 || length > COMPRESS_SEQUENCE_LENGTH) {
        fprintf(stderr, "create_binseq_key: Invalid parameters\n");
        return key;
    }

    key.binary_sequence = malloc(length);
    if (!key.binary_sequence) {
        fprintf(stderr, "create_binseq_key: Memory allocation failed\n");
        return key;
    }
    
    memcpy(key.binary_sequence, sequence, length);  // Always copy the data
    key.length = length;
    return key;
}


void free_key(BinSeqKey key) {
    if (key.binary_sequence) {
        free(key.binary_sequence);
    } else {
        fprintf(stderr, "Warning: Attempted to free NULL binary_sequence\n");
    }
}

BinSeqValue create_binseq_value(int frequency, const uint32_t* locations, uint16_t location_count) {
    BinSeqValue value = {0};
    value.frequency = frequency;
    value.seqLocationLength = location_count;
    value.seqLocationCapacity = location_count > 0 ? location_count : 4;

    if (location_count > 0) {
        value.seqLocation = malloc(value.seqLocationCapacity * sizeof(uint32_t));
        if (!value.seqLocation) {
            fprintf(stderr, "Failed to allocate seqLocation\n");
            value.seqLocationLength = 0;
            value.seqLocationCapacity = 0;
            return value;
        }
        memcpy(value.seqLocation, locations, location_count * sizeof(uint32_t));
    }
    // Else seqLocation remains NULL

    return value;
}

int binseq_value_append_location(BinSeqValue *value, uint32_t loc) {
    if (!value) return 0;

    if (value->seqLocationLength >= value->seqLocationCapacity) {
        uint16_t new_cap = value->seqLocationCapacity * 2; //exponential growth for better time complexity
        if (new_cap == 0) new_cap = 4;

        uint32_t *new_seq = realloc(value->seqLocation, new_cap * sizeof(uint32_t));
        if (!new_seq) return 0;

        value->seqLocation = new_seq;
        value->seqLocationCapacity = new_cap;
    }

    value->seqLocation[value->seqLocationLength++] = loc;
    return 1;
}


void free_binseq_value(BinSeqValue value) {
    if (value.seqLocation) {
        free(value.seqLocation);
    }
}


void binseq_map_print(BinSeqMap *map) {
    if (!map) {
        fprintf(stderr, "Debug Print: Map is NULL\n");
        return;
    }

    if (!map->entries) {
        fprintf(stderr, "Debug Print: Map entries are NULL\n");
        return;
    }

    printf("\n=== BinSeqMap Debug Print ===\n");
    printf("Map size: %zu, capacity: %zu\n", map->size, map->capacity);

    for (size_t i = 0; i < map->capacity; ++i) {
        Entry *entry = &map->entries[i];
        if (!entry || !entry->used) continue;

        BinSeqKey *key = &entry->key;
        BinSeqValue *val = &entry->value;

        printf("Entry %zu:\n", i);

        if (!key || !key->binary_sequence) {
            printf("  Key is NULL or invalid\n");
        } else {
            printf("  Key (length=%u): ", key->length);
            for (uint16_t j = 0; j < key->length; ++j) {
                printf("%02X, ", key->binary_sequence[j]);
            }
            printf("\n");
        }

        if (!val) {
            printf("  Value is NULL\n");
        } else {
            printf("  Value:\n");
            printf("    Frequency: %d\n", val->frequency);
            printf("    Locations (%u):", val->seqLocationLength);
            if (val->seqLocation) {
                for (uint16_t j = 0; j < val->seqLocationLength; ++j) {
                    printf(" %u, ", val->seqLocation[j]);
                }
            } else {
                printf(" (null)");
            }
            printf("\n");
        }
    }

    printf("=== End Debug Print ===\n");
}

