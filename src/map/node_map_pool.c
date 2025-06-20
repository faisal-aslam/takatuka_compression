// node_map_pool.c

#include "node_map_pool.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static BinSeqMap pool[MAX_LINK_MAPS];
static int current_index = 0;

BinSeqMap* node_map_pool_get_next() {
    if (current_index >= MAX_LINK_MAPS) {
        current_index = 0; //it is circular
    }

    BinSeqMap* entry = &pool[current_index++];
    return entry;
}

int get_current_pool_index() {
    return current_index;
}

BinSeqMap* node_map_pool_find(int16_t map_index) {
    if (map_index >= MAX_LINK_MAPS || map_index < 0) {
        fprintf(stderr, "Invalid map_index");
        return NULL;
    }
    return &pool[map_index]; 
}

void node_map_pool_reset() {
    for (int i = 0; i < MAX_LINK_MAPS; i++) {
        binseq_map_reset(&pool[i]);  // Frees internal keys
    }
    current_index = 0;
}


/*
--------At block start:
node_map_pool_reset();

--------- While building new level:

NodeMapEntry* child = node_map_pool_get(new_node_id);
NodeMapEntry* parent = node_map_pool_find(parent_id);

child->map = parent->map;  // Shallow copy: internal pointer reused
binseq_map_put(&child->map, new_seq, new_len, freq);


*/