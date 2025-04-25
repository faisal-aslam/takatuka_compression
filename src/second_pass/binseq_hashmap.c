// ----> binseq_hashmap.c 
#include "binseq_hashmap.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "xxhash.h"
#include "tree_node.h"

// Internal structures
typedef struct {
    uint8_t* binary_sequence;  // Key part
    uint16_t length;           // Key part
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
                  int value_frequency) {
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
            
            // Set value
            entry->frequency = value_frequency;
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

int binseq_map_increment_frequency(BinSeqMap* map, 
                                 const uint8_t* key_sequence, uint16_t key_length) {
    Entry* entry = find_entry(map, key_sequence, key_length);
    if (!entry) {
        return 0; //entry does not exist
    }
    
    entry->frequency++;
    return 1;
}

size_t binseq_map_size(const BinSeqMap* map) {
    return map ? map->size : 0;
}

size_t binseq_map_capacity(const BinSeqMap* map) {
    return map ? map->capacity : 0;
}

void binseq_map_print(const BinSeqMap* map) {
    if (!map || !map->entries) {
        fprintf(stderr, "Map is NULL or corrupted\n");
        return;
    }
    
    printf("Map (size=%zu, capacity=%zu):\n", map->size, map->capacity);
    for (size_t i = 0; i < map->capacity; i++) {
        const Entry* entry = &map->entries[i];
        if (!entry->used) continue;
        
        printf("  [%zu] Key (len=%u): ", i, entry->length);
        for (uint16_t j = 0; j < entry->length; j++) {
            printf("%02X ", entry->binary_sequence[j]);
        }
        printf("\n    Freq: %d\n", entry->frequency);
    }
}

int binseq_map_copy_to_node(const BinSeqMap* source, struct TreeNode* target) {
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

        if (!src->binary_sequence || src->length == 0) {
            fprintf(stderr, "Skipping invalid map entry\n");
            continue;
        }

        if (!binseq_map_put(new_map, src->binary_sequence, src->length, src->frequency)) {
            fprintf(stderr, "Failed to put entry in new map\n");
            binseq_map_free(new_map);
            return 0;
        }
    }

    if (target->map) {
        binseq_map_free(target->map);
    }
    target->map = new_map;
    return 1;
}