#ifndef SEQUENCE_REPOSITORY_USELESS_H
#define SEQUENCE_REPOSITORY_USELESS_H

#include <stdint.h>
#include <stddef.h>
#include "sequence_repository.h"

void seq_repo_init(SequenceRepository *repo);
void seq_repo_cleanup(SequenceRepository *repo);
void seq_repo_reset(SequenceRepository *repo);

// Add a sequence with a given node_id. If already present and node_id differs, mark as shared.
void seq_repo_add(SequenceRepository *repo, const uint8_t* data, uint16_t length, uint32_t node_id);

// Look up node_id for a given sequence. Returns UINT32_MAX_VALUE if not found or shared.
uint32_t seq_repo_get_node_id(SequenceRepository *repo, const uint8_t* data, uint16_t length);

#endif
