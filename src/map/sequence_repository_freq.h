#ifndef SEQUENCE_REPOSITORY_FREQ_H
#define SEQUENCE_REPOSITORY_FREQ_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "sequence_repository.h"

uint32_t seq_repo_get_frequency(SequenceRepository *repo, const uint8_t* data, uint16_t length);

// Returns new frequency, and optionally outputs index
uint32_t seq_repo_increase_frequency_cached(SequenceRepository *repo, const uint8_t* data, uint16_t len, uint32_t node_id);

// Fast path: uses cached index (O(1))
uint32_t seq_repo_decrease_by_index(SequenceRepository *repo, uint32_t node_id);

uint32_t seq_repo_increase_frequency(SequenceRepository *repo, const uint8_t* data, uint16_t len);

#endif
