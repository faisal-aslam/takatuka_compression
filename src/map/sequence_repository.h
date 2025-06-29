#ifndef SEQUENCE_REPOSITORY_H
#define SEQUENCE_REPOSITORY_H

#include <stdint.h>
#include <stddef.h>

#define UINT32_MAX_VALUE ((uint32_t)(-1))

typedef struct {
    uint8_t* data;
    uint16_t length;
} SequenceEntry;

typedef struct {
    SequenceEntry* entries;
    uint32_t* node_ids;         // Either a specific node_id or UINT32_MAX_VALUE
    uint64_t* hash_values;      // For open-addressing hash matching
    uint32_t capacity;
    uint32_t count;
} SequenceRepository;

void seq_repo_init();
void seq_repo_cleanup();

// Add a sequence with a given node_id. If already present and node_id differs, mark as shared.
void seq_repo_add(const uint8_t* data, uint16_t length, uint32_t node_id);

// Look up node_id for a given sequence. Returns UINT32_MAX_VALUE if not found or shared.
uint32_t seq_repo_get_node_id(const uint8_t* data, uint16_t length);

void remove_single_sequence_nodes();
#endif
