#include "node_map_pool.h"
#include <string.h>

static NodeMapEntry pool[MAX_LINK_MAPS];
static int current_index = 0;

NodeMapEntry* node_map_pool_get(int node_id) {
    if (current_index >= MAX_LINK_MAPS) return NULL;

    NodeMapEntry* entry = &pool[current_index++];
    entry->node_id = node_id;
    return entry;
}

NodeMapEntry* node_map_pool_find(int node_id) {
    for (int i = 0; i < current_index; i++) {
        if (pool[i].node_id == node_id) return &pool[i];
    }
    return NULL;
}

void node_map_pool_reset() {
    for (int i = 0; i < MAX_LINK_MAPS; i++) {
        binseq_map_reset(&pool[i].map);  // Frees internal keys
    }
    current_index = 0;
}


/*
--------At block start:
node_map_pool_cleanup();

--------- While building new level:

NodeMapEntry* parent = node_map_pool_find(parent_id);
NodeMapEntry* child = node_map_pool_get(new_node_id);
child->map = parent->map;  // Shallow copy: internal pointer reused
binseq_map_put(&child->map, new_seq, new_len, freq);


*/