//seq_freq_map.h

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "constants.h"

#define SEQ_MAP_CAPACITY ((TOTAL_GRAPH_NODES(SEQ_LENGTH_LIMIT, BLOCK_SIZE) * 3) / 2)

void init_seq_freq_map(void);


uint32_t seq_freq_decrement(const uint8_t *seq, uint8_t len);
uint32_t seq_freq_increment(const uint8_t *seq, uint8_t len, uint32_t node_id);
uint32_t seq_freq_set(const uint8_t *seq, uint8_t len, uint32_t freq, uint32_t node_id);
bool seq_freq_get(const uint8_t *seq, uint8_t len, uint32_t *out_freq, uint32_t *out_node_id);
uint32_t seq_freq_get_with_index(const uint8_t *seq, uint8_t len, uint32_t *out_freq, uint32_t *out_node_id);
uint32_t seq_freq_increment_with_index(uint32_t idx, uint32_t node_id);
uint32_t seq_freq_decrement_with_index(uint32_t idx);
uint32_t seq_freq_set_existing(uint32_t idx, uint32_t freq, uint32_t node_id);
void seq_freq_map_print(void);
void seq_freq_set_all(uint32_t freq);
