#include "node_map_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


static NodeMapEntry node_maps[MAX_NODE_MAPS];
static size_t node_map_count = 0;

void node_map_manager_init(void) {
    node_map_count = 0;
}

void node_map_manager_clear(void) {
    for (size_t i = 0; i < node_map_count; ++i) {
        if (node_maps[i].map) {
            binseq_map_free(node_maps[i].map);
        }
    }
    node_map_count = 0;
}

void node_map_manager_add(int node_id, BinSeqMap* map) {
    if (node_map_count >= MAX_NODE_MAPS) {
        fprintf(stderr, "Node map overflow\n");
        return;
    }
    node_maps[node_map_count++] = (NodeMapEntry){.node_id = node_id, .map = map};
}

BinSeqMap* node_map_manager_get(int node_id) {
    for (size_t i = 0; i < node_map_count; ++i) {
        if (node_maps[i].node_id == node_id) {
            return node_maps[i].map;
        }
    }
    return NULL;
}

void node_map_manager_copy_all_to_next_level(const int* new_ids, size_t count) {
    NodeMapEntry new_node_maps[MAX_NODE_MAPS];
    if (count > MAX_NODE_MAPS) return;

    for (size_t i = 0; i < count; ++i) {
        BinSeqMap* src = node_maps[i].map;
        BinSeqMap* copy = binseq_map_create(src ? binseq_map_capacity(src) : 16);
        if (src && copy) {
            for (size_t j = 0; j < binseq_map_capacity(src); ++j) {
                const Entry* entry = &src->entries[j];
                if (entry->used) {
                    binseq_map_put(copy, entry->binary_sequence, entry->length, entry->frequency);
                }
            }
        }
        new_node_maps[i] = (NodeMapEntry){.node_id = new_ids[i], .map = copy};
    }

    node_map_manager_clear();
    memcpy(node_maps, new_node_maps, count * sizeof(NodeMapEntry));
    node_map_count = count;
}
