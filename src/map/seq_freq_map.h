//seq_freq_map.h

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "constants.h"

#define SEQ_MAP_CAPACITY ((TOTAL_GRAPH_NODES(SEQ_LENGTH_LIMIT, BLOCK_SIZE) * 3) / 2) // Fixed capacity, no reallocs

// Public API (all use singleton internally)
void init_seq_freq_map(void);

uint32_t seq_freq_increment(const uint8_t *seq, uint8_t len);
uint32_t seq_freq_decrement(const uint8_t *seq, uint8_t len);
uint32_t seq_freq_get(const uint8_t *seq, uint8_t len);
uint32_t seq_freq_set(const uint8_t *seq, uint8_t len, uint32_t freq);
uint8_t seq_usefull_get(const uint8_t *seq, uint8_t len);
void seq_usefull_set(const uint8_t *seq, uint8_t len, uint8_t usefull);


