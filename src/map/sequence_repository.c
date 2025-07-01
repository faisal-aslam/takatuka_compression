#include "sequence_repository_useless.h"
#include "xxhash.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "graph/graph.h"

#define INITIAL_CAPACITY 1024
#define LOAD_FACTOR 0.75
#define GROWTH_FACTOR 2

typedef struct {
    const uint8_t* data;  // points to external block memory
    uint16_t length;
} SequenceEntry;

typedef struct {
    SequenceEntry* entries;
    uint64_t* hash_values;
    uint32_t* values;      // node_id or frequency
    uint8_t* is_used;
    uint32_t capacity;
    uint32_t count;
} SequenceRepository;

static SequenceRepository repo;
static int repo_initialized = 0;

void seq_repo_init() {
    if (repo_initialized) return;
    repo_initialized = 1;
    repo.capacity = INITIAL_CAPACITY;
    repo.count = 0;
    repo.entries = calloc(repo.capacity, sizeof(SequenceEntry));
    repo.hash_values = calloc(repo.capacity, sizeof(uint64_t));
    repo.values = calloc(repo.capacity, sizeof(uint32_t));
    repo.is_used = calloc(repo.capacity, sizeof(uint8_t));
    assert(repo.entries && repo.values && repo.hash_values && repo.is_used);
}

void seq_repo_cleanup() {
    repo_initialized = 0;
    free(repo.entries);
    free(repo.hash_values);
    free(repo.values);
    free(repo.is_used);
    memset(&repo, 0, sizeof(repo));
}

void seq_repo_reset() {
    memset(repo.entries, 0, sizeof(SequenceEntry) * repo.capacity);
    memset(repo.hash_values, 0, sizeof(uint64_t) * repo.capacity);
    memset(repo.values, 0, sizeof(uint32_t) * repo.capacity);
    memset(repo.is_used, 0, sizeof(uint8_t) * repo.capacity);
    repo.count = 0;
}

static void resize_repository() {
    uint32_t old_capacity = repo.capacity;
    SequenceEntry* old_entries = repo.entries;
    uint64_t* old_hash_values = repo.hash_values;
    uint32_t* old_values = repo.values;
    uint8_t* old_is_used = repo.is_used;

    repo.capacity *= GROWTH_FACTOR;
    repo.count = 0;
    repo.entries = calloc(repo.capacity, sizeof(SequenceEntry));
    repo.hash_values = calloc(repo.capacity, sizeof(uint64_t));
    repo.values = calloc(repo.capacity, sizeof(uint32_t));
    repo.is_used = calloc(repo.capacity, sizeof(uint8_t));
    assert(repo.entries && repo.hash_values && repo.values && repo.is_used);

    for (uint32_t i = 0; i < old_capacity; i++) {
        if (old_is_used[i]) {
            const uint8_t* data = old_entries[i].data;
            uint16_t len = old_entries[i].length;
            uint64_t hash = old_hash_values[i];
            uint32_t index = hash % repo.capacity;

            while (repo.is_used[index]) {
                index = (index + 1) % repo.capacity;
            }
            repo.entries[index].data = data;
            repo.entries[index].length = len;
            repo.hash_values[index] = hash;
            repo.values[index] = old_values[i];
            repo.is_used[index] = 1;
            repo.count++;
        }
    }

    free(old_entries);
    free(old_hash_values);
    free(old_values);
    free(old_is_used);
}

void seq_repo_add(const uint8_t* data, uint16_t length, uint32_t node_id) {
    if (!data || length <= 1) return;
    if (repo.count >= repo.capacity * LOAD_FACTOR) resize_repository();

    uint64_t hash = XXH3_64bits(data, length);
    uint32_t index = hash % repo.capacity;
    uint32_t start = index;

    do {
        if (!repo.is_used[index]) {
            repo.entries[index].data = data;
            repo.entries[index].length = length;
            repo.hash_values[index] = hash;
            repo.values[index] = node_id;
            repo.is_used[index] = 1;
            repo.count++;
            return;
        }
        if (repo.hash_values[index] == hash &&
            repo.entries[index].length == length &&
            memcmp(repo.entries[index].data, data, length) == 0) {
            if (repo.values[index] != node_id) {
                repo.values[index] = UINT32_MAX;
            }
            return;
        }
        index = (index + 1) % repo.capacity;
    } while (index != start);
}

uint32_t seq_repo_get_node_id(const uint8_t* data, uint16_t length) {
    if (!data || length <= 1) return UINT32_MAX;
    uint64_t hash = XXH3_64bits(data, length);
    uint32_t index = hash % repo.capacity;
    uint32_t start = index;

    do {
        if (!repo.is_used[index]) return UINT32_MAX;
        if (repo.hash_values[index] == hash &&
            repo.entries[index].length == length &&
            memcmp(repo.entries[index].data, data, length) == 0) {
            return repo.values[index];
        }
        index = (index + 1) % repo.capacity;
    } while (index != start);

    return UINT32_MAX;
}

uint32_t seq_repo_get_frequency(const uint8_t* data, uint16_t length) {
    if (!data || length <= 1) return 0;
    uint64_t hash = XXH3_64bits(data, length);
    uint32_t index = hash % repo.capacity;
    uint32_t start = index;

    do {
        if (!repo.is_used[index]) return 0;
        if (repo.hash_values[index] == hash &&
            repo.entries[index].length == length &&
            memcmp(repo.entries[index].data, data, length) == 0) {
            return repo.values[index];
        }
        index = (index + 1) % repo.capacity;
    } while (index != start);

    return 0;
}

uint32_t seq_repo_increase_frequency(const uint8_t* data, uint16_t length) {
    if (!data || length <= 1) return 0;
    if (repo.count >= repo.capacity * LOAD_FACTOR) resize_repository();

    uint64_t hash = XXH3_64bits(data, length);
    uint32_t index = hash % repo.capacity;
    uint32_t start = index;

    do {
        if (!repo.is_used[index]) {
            repo.entries[index].data = data;
            repo.entries[index].length = length;
            repo.hash_values[index] = hash;
            repo.values[index] = 1;
            repo.is_used[index] = 1;
            repo.count++;
            return 1;
        }
        if (repo.hash_values[index] == hash &&
            repo.entries[index].length == length &&
            memcmp(repo.entries[index].data, data, length) == 0) {
            assert(repo.values[index] < UINT32_MAX);
            return ++repo.values[index];
        }
        index = (index + 1) % repo.capacity;
    } while (index != start);

    return 0;
}

uint32_t seq_repo_decrease_frequency(const uint8_t* data, uint16_t length) {
    if (!data || length <= 1) return 0;
    uint64_t hash = XXH3_64bits(data, length);
    uint32_t index = hash % repo.capacity;
    uint32_t start = index;

    do {
        if (!repo.is_used[index]) return 0;
        if (repo.hash_values[index] == hash &&
            repo.entries[index].length == length &&
            memcmp(repo.entries[index].data, data, length) == 0) {
            if (repo.values[index] > 0) repo.values[index]--;
            return repo.values[index];
        }
        index = (index + 1) % repo.capacity;
    } while (index != start);

    return 0;
}
