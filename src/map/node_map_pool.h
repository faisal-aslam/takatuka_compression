#ifndef NODE_MAP_POOL_H
#define NODE_MAP_POOL_H

#include <stdint.h>
#include "binseq_hashmap.h"  // For BinSeqMap

#define MAX_LINK_MAPS (2*SEQ_LENGTH_LIMIT * SEQ_LENGTH_LIMIT)

// Returns a fresh BinSeqMap* for a new node
BinSeqMap* node_map_pool_get_next();

// Returns pointer to map for a given map_index
BinSeqMap* node_map_pool_find(uint16_t map_index);

// reset the pool without freeing (static) memory.
void node_map_pool_reset();

int get_current_pool_index();
#endif
