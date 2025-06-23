#ifndef SEQUENCE_REPOSITORY_H
#define SEQUENCE_REPOSITORY_H

#include <stdint.h>
#include "../constants.h"

typedef struct {
    uint8_t* data;
    uint16_t length;
} SequenceEntry;

typedef struct {
    SequenceEntry* sequences;
    uint32_t* hash_values;
    uint32_t capacity;
    uint32_t count;
} SequenceRepository;

void seq_repo_init(SequenceRepository* repo);
void seq_repo_cleanup(SequenceRepository* repo);

// Returns sequence ID (creates new entry if needed)
uint32_t seq_repo_add(SequenceRepository* repo, 
                     const uint8_t* sequence, 
                     uint16_t length);

// Gets sequence by ID (returns NULL if invalid)
const uint8_t* seq_repo_get_data(SequenceRepository* repo, uint32_t id);
uint16_t seq_repo_get_length(SequenceRepository* repo, uint32_t id);

#endif