// ----> binseq_hashmap.c 
#include "binseq_hashmap.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "xxhash.h"
#include "tree_node.h"

// Internal structures
// In binseq_hashmap.h
typedef struct {
    uint8_t* binary_sequence;  // Key part
    uint16_t length;           // Key part
    uint32_t* locations;       // Value part
    uint16_t location_count;
    uint16_t location_capacity;
    int frequency;             // Value part
    int used;
} Entry;

struct BinSeqMap {
    Entry* entries;
    size_t capacity;
    size_t size;
};

// Helper functions
static uint64_t hash_sequence(const uint8_t* sequence, uint16_t length) {
    if (!sequence || length == 0) return 0;
    return XXH3_64bits(sequence, length);
}

static int sequences_equal(const uint8_t* a, uint16_t a_len, 
                         const uint8_t* b, uint16_t b_len) {
    if (a_len != b_len) return 0;
    return memcmp(a, b, a_len) == 0;
}

static Entry* find_entry(const BinSeqMap* map, 
                        const uint8_t* sequence, uint16_t length) {
    if (!map || !sequence || length == 0) return NULL;
    
    uint64_t hash = hash_sequence(sequence, length);
    size_t index = hash % map->capacity;
    
    for (size_t i = 0; i < map->capacity; i++) {
        size_t try = (index + i) % map->capacity;
        Entry* entry = &map->entries[try];
        
        if (!entry->used) return NULL;
        if (sequences_equal(entry->binary_sequence, entry->length, 
                           sequence, length)) {
            return entry;
        }
    }
    
    return NULL;
}

static int resize_map(BinSeqMap* map, size_t new_capacity) {
    if (!map || new_capacity <= map->size) return 0;
    
    Entry* new_entries = calloc(new_capacity, sizeof(Entry));
    if (!new_entries) return 0;
    
    // Rehash all entries
    for (size_t i = 0; i < map->capacity; i++) {
        Entry* old = &map->entries[i];
        if (!old->used) continue;
        
        uint64_t hash = hash_sequence(old->binary_sequence, old->length);
        size_t index = hash % new_capacity;
        
        // Find empty slot
        for (size_t try = 0; try < new_capacity; try++) {
            size_t pos = (index + try) % new_capacity;
            if (!new_entries[pos].used) {
                new_entries[pos] = *old;
                break;
            }
        }
    }
    
    free(map->entries);
    map->entries = new_entries;
    map->capacity = new_capacity;
    return 1;
}

// Public interface implementation
BinSeqMap* binseq_map_create(size_t initial_capacity) {
    if (initial_capacity == 0) initial_capacity = 16;
    
    BinSeqMap* map = calloc(1, sizeof(BinSeqMap));
    if (!map) {
        fprintf(stderr, "\n Unable to create map \n");
        return NULL;
    }
    
    map->entries = calloc(initial_capacity, sizeof(Entry));
    if (!map->entries) {
        free(map);
        return NULL;
    }
    
    map->capacity = initial_capacity;
    map->size = 0;
    return map;
}

void binseq_map_free(BinSeqMap* map) {
    if (!map) return;
    
    if (map->entries) {
        for (size_t i = 0; i < map->capacity; i++) {
            Entry* entry = &map->entries[i];
            if (entry->used) {
                free(entry->binary_sequence);
                free(entry->locations);
                // Clear the entry
                memset(entry, 0, sizeof(Entry));
            }
        }
        free(map->entries);
        map->entries = NULL;
    }
    
    free(map);
}

int binseq_map_put(BinSeqMap* map, 
                  const uint8_t* key_sequence, uint16_t key_length,
                  int value_frequency, const uint32_t* value_locations, uint16_t value_location_count) {
    if (!map || !key_sequence || key_length == 0) return 0;
    
    // Check if resize needed (70% load factor)
    if (map->size * 10 > map->capacity * 7) {
        if (!resize_map(map, map->capacity * 2)) return 0;
    }
    
    // Check if key exists
    Entry* existing = find_entry(map, key_sequence, key_length);
    if (existing) {
        // Update existing entry
        existing->frequency = value_frequency;
        
        // Free old locations if they exist
        free(existing->locations);
        
        // Copy new locations
        if (value_location_count > 0) {
            existing->locations = malloc(value_location_count * sizeof(uint32_t));
            if (!existing->locations) return 0;
            memcpy(existing->locations, value_locations, value_location_count * sizeof(uint32_t));
        } else {
            existing->locations = NULL;
        }
        
        existing->location_count = value_location_count;
        existing->location_capacity = value_location_count;
        return 1;
    }
    
    // Create new entry
    uint64_t hash = hash_sequence(key_sequence, key_length);
    size_t index = hash % map->capacity;
    
    for (size_t i = 0; i < map->capacity; i++) {
        size_t try = (index + i) % map->capacity;
        Entry* entry = &map->entries[try];
        
        if (!entry->used) {
            // Copy key
            entry->binary_sequence = malloc(key_length);
            if (!entry->binary_sequence) return 0;
            memcpy(entry->binary_sequence, key_sequence, key_length);
            entry->length = key_length;
            
            // Copy value
            entry->frequency = value_frequency;
            if (value_location_count > 0) {
                entry->locations = malloc(value_location_count * sizeof(uint32_t));
                if (!entry->locations) {
                    free(entry->binary_sequence);
                    return 0;
                }
                memcpy(entry->locations, value_locations, value_location_count * sizeof(uint32_t));
            } else {
                entry->locations = NULL;
            }
            
            entry->location_count = value_location_count;
            entry->location_capacity = value_location_count;
            entry->used = 1;
            map->size++;
            return 1;
        }
    }
    
    return 0;
}

const int* binseq_map_get_frequency(const BinSeqMap* map, 
                                   const uint8_t* key_sequence, uint16_t key_length) {
    Entry* entry = find_entry(map, key_sequence, key_length);
    return entry ? &entry->frequency : NULL;
}

const uint32_t* binseq_map_get_locations(const BinSeqMap* map, 
                                       const uint8_t* key_sequence, uint16_t key_length,
                                       uint16_t* out_location_count) {
    Entry* entry = find_entry(map, key_sequence, key_length);
    if (!entry) return NULL;
    
    if (out_location_count) *out_location_count = entry->location_count;
    return entry->locations;
}

int binseq_map_append_location(BinSeqMap* map, 
                             const uint8_t* key_sequence, uint16_t key_length,
                             uint32_t new_location) {
    Entry* entry = find_entry(map, key_sequence, key_length);
    if (!entry) return 0;
    
    // Resize if needed
    if (entry->location_count >= entry->location_capacity) {
        uint16_t new_cap = entry->location_capacity ? entry->location_capacity * 2 : 4;
        uint32_t* new_locations = realloc(entry->locations, new_cap * sizeof(uint32_t));
        if (!new_locations) return 0;
        
        entry->locations = new_locations;
        entry->location_capacity = new_cap;
    }
    
    entry->locations[entry->location_count++] = new_location;
    entry->frequency++; // Assuming each location represents an occurrence
    return 1;
}

size_t binseq_map_size(const BinSeqMap* map) {
    return map ? map->size : 0;
}

size_t binseq_map_capacity(const BinSeqMap* map) {
    return map ? map->capacity : 0;
}

void binseq_map_print(const BinSeqMap* map) {
    if (!map || !map->entries) {  // Add check for entries
        fprintf(stderr, "Map is NULL or corrupted\n");
        return;
    }
    
    printf("Map (size=%zu, capacity=%zu):\n", map->size, map->capacity);
    for (size_t i = 0; i < map->capacity; i++) {
        const Entry* entry = &map->entries[i];
        if (!entry || !entry->used) continue;  // Skip NULL or unused entries
        
        printf("  [%zu] Key (len=%u): ", i, entry->length);
        for (uint16_t j = 0; j < entry->length; j++) {
            printf("%02X ", entry->binary_sequence[j]);
        }
        printf("\n    Freq: %d, Locations (%u): ", entry->frequency, entry->location_count);
        for (uint16_t j = 0; j < entry->location_count; j++) {
            printf("%u ", entry->locations[j]);
        }
        printf("\n");
    }
}

int binseq_map_copy_to_node(const BinSeqMap* source, struct TreeNode* target) {
    // More thorough validation
    if (!source || !target || !source->entries || source->capacity == 0 || source->size == 0) {
        fprintf(stderr, "Invalid source map or target node\n");
        return 0;
    }

    BinSeqMap* new_map = binseq_map_create(source->capacity);
    if (!new_map) {
        fprintf(stderr, "Failed to create new map\n");
        return 0;
    }

    for (size_t i = 0; i < source->capacity; i++) {
        const Entry* src = &source->entries[i];
        if (!src->used) continue;

        // Skip invalid entries
        if (!src->binary_sequence || src->length == 0) {
            fprintf(stderr, "Skipping invalid map entry\n");
            continue;
        }

        uint32_t* locs_copy = malloc(src->location_count * sizeof(uint32_t));
        if (!locs_copy) {
            fprintf(stderr, "Failed to allocate locations copy\n");
            binseq_map_free(new_map);
            return 0;
        }
        memcpy(locs_copy, src->locations, src->location_count * sizeof(uint32_t));

        if (!binseq_map_put(new_map, src->binary_sequence, src->length,
                          src->frequency, locs_copy, src->location_count)) {
            fprintf(stderr, "Failed to put entry in new map\n");
            free(locs_copy);
            binseq_map_free(new_map);
            return 0;
        }
        free(locs_copy);
    }

    // Only free old map after successful copy
    if (target->map) {
        binseq_map_free(target->map);
    }
    target->map = new_map;
    return 1;
}