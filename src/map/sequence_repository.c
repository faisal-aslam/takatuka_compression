// sequence_repository.c
#include "sequence_repository.h"
#include "xxhash.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "graph/graph.h"

#define INITIAL_CAPACITY 1024
#define LOAD_FACTOR 0.75
#define GROWTH_FACTOR 2

static SequenceRepository repo;
static int repo_initialized = 0;

void seq_repo_init() {
    if (repo_initialized) return;
    repo_initialized = 1;
    repo.capacity = INITIAL_CAPACITY;
    repo.count = 0;
    repo.entries = calloc(repo.capacity, sizeof(SequenceEntry));
    repo.node_ids = calloc(repo.capacity, sizeof(uint32_t));
    repo.hash_values = calloc(repo.capacity, sizeof(uint64_t));
    assert(repo.entries && repo.node_ids && repo.hash_values);
}

void seq_repo_cleanup() {
    repo_initialized = 0;
    for (uint32_t i = 0; i < repo.capacity; i++) {
        if (repo.entries[i].data) {
            free(repo.entries[i].data);
        }
    }
    free(repo.entries);
    free(repo.node_ids);
    free(repo.hash_values);
    memset(&repo, 0, sizeof(repo));
}

static void resize_repository() {
    uint32_t old_capacity = repo.capacity;
    SequenceEntry* old_entries = repo.entries;
    uint32_t* old_node_ids = repo.node_ids;
    uint64_t* old_hash_values = repo.hash_values;

    repo.capacity *= GROWTH_FACTOR;
    repo.count = 0;
    repo.entries = calloc(repo.capacity, sizeof(SequenceEntry));
    repo.node_ids = calloc(repo.capacity, sizeof(uint32_t));
    repo.hash_values = calloc(repo.capacity, sizeof(uint64_t));
    assert(repo.entries && repo.node_ids && repo.hash_values);

    for (uint32_t i = 0; i < old_capacity; i++) {
        if (old_entries[i].data) {
            uint64_t hash = old_hash_values[i];
            uint32_t index = hash % repo.capacity;
            while (repo.entries[index].data) {
                index = (index + 1) % repo.capacity;
            }
            repo.entries[index] = old_entries[i];
            repo.node_ids[index] = old_node_ids[i];
            repo.hash_values[index] = hash;
            repo.count++;
        }
    }
    free(old_entries);
    free(old_node_ids);
    free(old_hash_values);
}

void seq_repo_add(const uint8_t* data, uint16_t length, uint32_t node_id) {
    if (!data || length == 0) return;
    if (repo.count >= repo.capacity * LOAD_FACTOR) {
        resize_repository();
    }

    uint64_t hash = XXH3_64bits(data, length);
    uint32_t index = hash % repo.capacity;
    uint32_t start = index;

    do {
        if (!repo.entries[index].data) {
            uint8_t* copy = malloc(length);
            if (!copy) return;
            memcpy(copy, data, length);
            repo.entries[index].data = copy;
            repo.entries[index].length = length;
            repo.node_ids[index] = node_id;
            repo.hash_values[index] = hash;
            repo.count++;
            return;
        }
        if (repo.hash_values[index] == hash &&
            repo.entries[index].length == length &&
            memcmp(repo.entries[index].data, data, length) == 0) {
            if (repo.node_ids[index] != node_id) {
                repo.node_ids[index] = UINT32_MAX_VALUE;
            }
            return;
        }
        index = (index + 1) % repo.capacity;
    } while (index != start);
}

uint32_t seq_repo_get_node_id(const uint8_t* data, uint16_t length) {
    if (!data || length == 0) return UINT32_MAX_VALUE;

    uint64_t hash = XXH3_64bits(data, length);
    uint32_t index = hash % repo.capacity;
    uint32_t start = index;

    do {
        if (!repo.entries[index].data) return UINT32_MAX_VALUE;
        if (repo.hash_values[index] == hash &&
            repo.entries[index].length == length &&
            memcmp(repo.entries[index].data, data, length) == 0) {
            return repo.node_ids[index];
        }
        index = (index + 1) % repo.capacity;
    } while (index != start);

    return UINT32_MAX_VALUE;
}



void remove_single_sequence_nodes() {
    for (uint32_t i = 0; i < repo.capacity; i++) {
        if (!repo.entries[i].data) continue;

        uint32_t node_id = repo.node_ids[i];
        if (node_id != UINT32_MAX_VALUE) {
            GraphNode* g_node = get_graph_node(node_id);
            if (g_node) g_node->isUseless = 1;
        }
    }
}
