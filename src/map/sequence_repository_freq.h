#ifndef SEQUENCE_REPOSITORY_FREQ_H
#define SEQUENCE_REPOSITORY_FREQ_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

void seq_repo_init();
void seq_repo_cleanup();
void seq_repo_reset();

uint32_t seq_repo_increase_frequency(const uint8_t* data, uint16_t length);
uint32_t seq_repo_decrease_frequency(const uint8_t* data, uint16_t length);
uint32_t seq_repo_get_frequency(const uint8_t* data, uint16_t length);

// Returns new frequency, and optionally outputs index
uint32_t seq_repo_increase_frequency_cached(const uint8_t* data, uint16_t len, uint32_t* out_index);

// Fast path: uses cached index (O(1))
uint32_t seq_repo_decrease_by_index(uint32_t index);


#endif
