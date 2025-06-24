// sequence_repository.c

#include "sequence_repository.h"
#include "xxhash.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define INITIAL_CAPACITY 1024
#define LOAD_FACTOR 0.75
#define GROWTH_FACTOR 2

void seq_repo_init(SequenceRepository* repo) {
    repo->capacity = INITIAL_CAPACITY;
    repo->count = 0;
    repo->sequences = calloc(repo->capacity, sizeof(SequenceEntry));
    repo->hash_values = calloc(repo->capacity, sizeof(uint32_t));
    assert(repo->sequences && repo->hash_values); // Check allocation success
}

void seq_repo_cleanup(SequenceRepository* repo) {
    if (!repo) return;
    
    for (uint32_t i = 0; i < repo->capacity; i++) {
        if (repo->sequences[i].data) {
            free(repo->sequences[i].data);
        }
    }
    free(repo->sequences);
    free(repo->hash_values);
    memset(repo, 0, sizeof(*repo)); // Clear all fields
}

static void resize_repository(SequenceRepository* repo) {
    uint32_t old_capacity = repo->capacity;
    SequenceEntry* old_sequences = repo->sequences;
    uint32_t* old_hashes = repo->hash_values;
    
    // Calculate new capacity
    repo->capacity = old_capacity * GROWTH_FACTOR;
    repo->count = 0;
    repo->sequences = calloc(repo->capacity, sizeof(SequenceEntry));
    repo->hash_values = calloc(repo->capacity, sizeof(uint32_t));
    assert(repo->sequences && repo->hash_values);
    
    // Rehash all existing entries
    for (uint32_t i = 0; i < old_capacity; i++) {
        if (old_sequences[i].data) {
            uint64_t hash = old_hashes[i];
           
            // Find new position in resized table
            uint32_t new_index = hash % repo->capacity;
            while (repo->sequences[new_index].data) {
                new_index = (new_index + 1) % repo->capacity;
            }
            
            // Insert into new table
            repo->sequences[new_index] = old_sequences[i];
            repo->hash_values[new_index] = hash;
            repo->count++;
        }
    }
    
    free(old_sequences);
    free(old_hashes);
}

static uint32_t find_or_add(SequenceRepository* repo, 
                          const uint8_t* sequence, 
                          uint16_t length,
                          uint64_t hash) {
    // Check if we need to resize
    if (repo->count >= repo->capacity * LOAD_FACTOR) {
        resize_repository(repo);
    }
    
    uint32_t index = hash % repo->capacity;
    uint32_t start_index = index;
    
    do {
        if (repo->sequences[index].data == NULL) {
            // Found empty slot
            uint8_t* copy = malloc(length);
            if (!copy) return UINT32_MAX; // Allocation failed
            
            memcpy(copy, sequence, length);
            repo->sequences[index].data = copy;
            repo->sequences[index].length = length;
            repo->hash_values[index] = hash;
            repo->count++;
            return index;
        }
        
        // Check for existing match
        if (repo->hash_values[index] == hash &&
            repo->sequences[index].length == length &&
            memcmp(repo->sequences[index].data, sequence, length) == 0) {
            return index;
        }
        
        // Linear probing
        index = (index + 1) % repo->capacity;
    } while (index != start_index); // Prevent infinite loops
    
    // If we get here, the table is full (shouldn't happen due to resize)
    resize_repository(repo);
    return find_or_add(repo, sequence, length, hash); // Retry with larger table
}

uint32_t seq_repo_add(SequenceRepository* repo, 
                     const uint8_t* sequence, 
                     uint16_t length) {
    if (!repo || !sequence || length == 0) return UINT32_MAX;
    
    uint64_t hash = XXH3_64bits(sequence, length);
    return find_or_add(repo, sequence, length, hash);
}

const uint8_t* seq_repo_get_data(SequenceRepository* repo, uint32_t id) {
    if (!repo || id >= repo->capacity) return NULL;
    return repo->sequences[id].data;
}

uint16_t seq_repo_get_length(SequenceRepository* repo, uint32_t id) {
    if (!repo || id >= repo->capacity) return 0;
    return repo->sequences[id].length;
}