#pragma once

#include <stdint.h>

#define MAX_TOP_SAVINGS 100

#define MAX_NODES_PER_SEQ 64

typedef struct {
    const uint8_t *sequence; // pointer to the sequence (in block)
    uint8_t length;
    uint32_t frequency;
    uint32_t savings;

    // Node references
    uint32_t node_ids[MAX_NODES_PER_SEQ];
    uint16_t node_count;
} TopSavingNode;

void init_top_savings(void);
void try_insert_top_saving(const uint8_t *seq, uint8_t len, uint32_t freq, uint32_t node_id);
void print_top_savings(void);
/**
 * Fills the given `out_ids` array with node IDs from top savings.
 * Returns number of IDs written (max out_count).
 */
int get_top_saving_node_ids(uint32_t *out_ids, int max_count);

