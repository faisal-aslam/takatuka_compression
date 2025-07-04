#ifndef SEQUENCE_REPOSITIRY_H
#define SEQUENCE_REPOSITIRY_H

#include "xxhash.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "graph/graph.h"

#define INITIAL_CAPACITY 1024
#define LOAD_FACTOR 0.75
#define GROWTH_FACTOR 2

typedef struct {
    const uint8_t* data;  // points to external block memory
    uint16_t length;
} SequenceEntry;

typedef struct {
    SequenceEntry* entries;
    uint64_t* hash_values;
    uint32_t* values;      // node_id or frequency
    uint8_t* is_used;
    uint32_t capacity;
    uint32_t count;
} SequenceRepository;

void seq_repo_print_all(SequenceRepository *repo);
void seq_repo_init(SequenceRepository *repo, uint32_t inital_capcity);
void seq_repo_cleanup(SequenceRepository *repo);
void seq_repo_reset(SequenceRepository *repo);

#endif