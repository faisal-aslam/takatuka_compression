#ifndef NODE_MAP_POOL_H
#define NODE_MAP_POOL_H

#include <stdint.h>
#include "binseq_hashmap.h"  // For BinSeqMap

#define MAX_LINK_MAPS (SEQ_LENGTH_LIMIT * SEQ_LENGTH_LIMIT)

typedef struct {
    int node_id;        // Will be updated when reused
    BinSeqMap map;      // Statically allocated internal map
} NodeMapEntry;


// Returns a fresh NodeMapEntry* for a new node
NodeMapEntry* node_map_pool_get(int node_id);

// Returns pointer to map for a given node_id
NodeMapEntry* node_map_pool_find(int node_id);

// reset the pool without freeing (static) memory.
void node_map_pool_reset();

#endif
