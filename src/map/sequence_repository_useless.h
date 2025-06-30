#ifndef SEQUENCE_REPOSITORY_USELESS_H
#define SEQUENCE_REPOSITORY_USELESS_H

#include <stdint.h>
#include <stddef.h>

void seq_repo_init();
void seq_repo_cleanup();
void seq_repo_reset();

// Add a sequence with a given node_id. If already present and node_id differs, mark as shared.
void seq_repo_add(const uint8_t* data, uint16_t length, uint32_t node_id);

// Look up node_id for a given sequence. Returns UINT32_MAX_VALUE if not found or shared.
uint32_t seq_repo_get_node_id(const uint8_t* data, uint16_t length);

#endif
