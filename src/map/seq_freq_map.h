// seq_freq_map.h

#pragma once

#include "constants.h"
#include <stdbool.h>
#include <stdint.h>

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

/* Bulk filter (keeps O(capacity) since it already scans), but it DOES NOT
 * recompute the best — it just marks cache invalid. You must call
 * seq_freq_recompute_best() yourself if you need an up-to-date best afterwards.
 */
void seq_freq_filter_freqs_and_length(uint32_t freq_to_set, uint32_t freq_to_delete, uint8_t len_to_delete);

/* Best-cache helpers (lazy invalidation model). */
void seq_freq_recompute_best(void);     /* slow: scans whole table and rebuilds best */
bool seq_freq_best_is_valid(void);      /* true if cache contains a valid best */
uint32_t seq_freq_get_best_index(void); /* UINT32_MAX if invalid */
bool seq_freq_get_best(const uint8_t **out_seq, uint8_t *out_len, uint32_t *out_freq, uint32_t *out_node_id);
