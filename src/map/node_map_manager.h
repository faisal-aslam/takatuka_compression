#ifndef NODE_MAP_MANAGER_H
#define NODE_MAP_MANAGER_H

#include <stdint.h>
#include "binseq_hashmap.h"

#define MAX_NODE_MAPS 1024  // configurable per level

typedef struct {
    int node_id;
    BinSeqMap* map;
} NodeMapEntry;

void node_map_manager_init(void);
void node_map_manager_clear(void);
void node_map_manager_add(int node_id, BinSeqMap* map);
BinSeqMap* node_map_manager_get(int node_id);
void node_map_manager_copy_all_to_next_level(const int* new_ids, size_t count);

#endif
