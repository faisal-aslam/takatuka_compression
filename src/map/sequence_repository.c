//This is sequence_repository.c

#include "sequence_repository.h"
#include "xxhash.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define INITIAL_CAPACITY 1024
#define LOAD_FACTOR 0.75

void seq_repo_init(SequenceRepository* repo) {
    repo->capacity = INITIAL_CAPACITY;
    repo->count = 0;
    repo->sequences = calloc(repo->capacity, sizeof(SequenceEntry));
    repo->hash_values = calloc(repo->capacity, sizeof(uint32_t));
}


void seq_repo_cleanup(SequenceRepository* repo) {
    for (uint32_t i = 0; i < repo->count; i++) {
        free(repo->sequences[i].data);
    }
    free(repo->sequences);//should I do that?
    free(repo->hash_values);
}

static uint32_t find_or_add(SequenceRepository* repo, 
                          const uint8_t* sequence, 
                          uint16_t length,
                          uint64_t hash) {
    //capacity should not be 0
    assert(repo->capacity > 0);
    
    // Linear probing hash table
    uint32_t index = hash % repo->capacity;
    
    while (1) {
        if (repo->sequences[index].data == NULL) {
            // Found empty slot - add new sequence
            uint8_t* copy = malloc(length);
            memcpy(copy, sequence, length);
            
            repo->sequences[index].data = copy;
            repo->sequences[index].length = length;
            repo->hash_values[index] = hash;
            repo->count++;
            return index;
        }
        
        // Check if existing sequence matches
        if (repo->hash_values[index] == hash &&
            repo->sequences[index].length == length &&
            memcmp(repo->sequences[index].data, sequence, length) == 0) {
            return index; // Return existing ID
        }
        
        // Collision - probe next slot
        index = (index + 1) % repo->capacity;
    }
}

uint32_t seq_repo_add(SequenceRepository* repo, 
                     const uint8_t* sequence, 
                     uint16_t length) {
    uint64_t hash = XXH3_64bits(sequence, length);
    return find_or_add(repo, sequence, length, hash);
}

const uint8_t* seq_repo_get_data(SequenceRepository* repo, uint32_t id) {
    if (id >= repo->capacity || repo->sequences[id].data == NULL) {
        return NULL;
    }
    return repo->sequences[id].data;
}

uint16_t seq_repo_get_length(SequenceRepository* repo, uint32_t id) {
    if (id >= repo->capacity || repo->sequences[id].data == NULL) {
        return 0;
    }
    return repo->sequences[id].length;
}