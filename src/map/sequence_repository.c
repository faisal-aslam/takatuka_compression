// sequence_repository.c
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
    uint8_t* data;
    uint16_t length;
} SequenceEntry;

typedef struct {
    SequenceEntry* entries;
    uint64_t* hash_values;      // For open-addressing hash matching
    uint32_t* values;           // can store node_id or frequency    
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
    repo.values = calloc(repo.capacity, sizeof(uint32_t));
    repo.hash_values = calloc(repo.capacity, sizeof(uint64_t));
    assert(repo.entries && repo.values && repo.hash_values);
}

void seq_repo_cleanup() {
    repo_initialized = 0;
    for (uint32_t i = 0; i < repo.capacity; i++) {
        if (repo.entries[i].data) {
            free(repo.entries[i].data);
        }
    }
    free(repo.entries);
    free(repo.values);
    free(repo.hash_values);
    memset(&repo, 0, sizeof(repo));
}

void seq_repo_reset() {
    for (uint32_t i = 0; i < repo.capacity; i++) {
        repo.values[i] = 0;
        repo.hash_values[i] = 0;
        if (repo.entries[i].data) {
            free(repo.entries[i].data);
            repo.entries[i].data = NULL;
            repo.entries[i].length = 0;
        }
    }
    repo.count = 0;
}


static void resize_repository() {
    uint32_t old_capacity = repo.capacity;
    SequenceEntry* old_entries = repo.entries;
    uint32_t* old_values = repo.values;
    uint64_t* old_hash_values = repo.hash_values;

    repo.capacity *= GROWTH_FACTOR;
    repo.count = 0;
    repo.entries = calloc(repo.capacity, sizeof(SequenceEntry));
    repo.values = calloc(repo.capacity, sizeof(uint32_t));
    repo.hash_values = calloc(repo.capacity, sizeof(uint64_t));
    assert(repo.entries && repo.values && repo.hash_values);

    for (uint32_t i = 0; i < old_capacity; i++) {
        if (old_entries[i].data) {
            uint64_t hash = old_hash_values[i];
            uint32_t index = hash % repo.capacity;
            while (repo.entries[index].data) {
                index = (index + 1) % repo.capacity;
            }
            repo.entries[index] = old_entries[i];
            repo.values[index] = old_values[i];
            repo.hash_values[index] = hash;
            repo.count++;
        }
    }
    free(old_entries);
    free(old_values);
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
            repo.values[index] = node_id;
            repo.hash_values[index] = hash;
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
    if (!data || length == 0) return UINT32_MAX;

    uint64_t hash = XXH3_64bits(data, length);
    uint32_t index = hash % repo.capacity;
    uint32_t start = index;

    do {
        if (!repo.entries[index].data) return UINT32_MAX;
        if (repo.hash_values[index] == hash &&
            repo.entries[index].length == length &&
            memcmp(repo.entries[index].data, data, length) == 0) {
            return repo.values[index];
        }
        index = (index + 1) % repo.capacity;
    } while (index != start);

    return UINT32_MAX;
}


// Returns the frequency for a sequence, or 0 if not found
uint32_t seq_repo_get_frequency(const uint8_t* data, uint16_t length) {
    if (!data || length == 0) return 0;
    uint64_t hash = XXH3_64bits(data, length);
    uint32_t index = hash % repo.capacity;
    uint32_t start = index;

    do {
        if (!repo.entries[index].data) return 0;
        if (repo.hash_values[index] == hash &&
            repo.entries[index].length == length &&
            memcmp(repo.entries[index].data, data, length) == 0) {
            return repo.values[index];  // frequency stored
        }
        index = (index + 1) % repo.capacity;
    } while (index != start);
    return 0;
}

uint32_t seq_repo_increase_frequency(const uint8_t* data, uint16_t length) {
    if (!data || length == 0) return 0;

    if (repo.count >= repo.capacity * LOAD_FACTOR) {
        resize_repository();
    }

    uint64_t hash = XXH3_64bits(data, length);
    uint32_t index = hash % repo.capacity;
    uint32_t start = index;

    do {
        if (!repo.entries[index].data) {
            // New entry
            uint8_t* copy = malloc(length);
            if (!copy) return 0;
            memcpy(copy, data, length);
            repo.entries[index].data = copy;
            repo.entries[index].length = length;
            repo.hash_values[index] = hash;
            repo.values[index] = 1;
            repo.count++;
            return 1;
        }

        if (repo.hash_values[index] == hash &&
            repo.entries[index].length == length &&
            memcmp(repo.entries[index].data, data, length) == 0) {
            assert(repo.values[index] < UINT32_MAX);  // Prevent overflow
            repo.values[index]++;
            return repo.values[index];
        }

        index = (index + 1) % repo.capacity;
    } while (index != start);

    return 0;  // Should not reach here
}

uint32_t seq_repo_decrease_frequency(const uint8_t* data, uint16_t length) {
    if (!data || length == 0) return 0;

    uint64_t hash = XXH3_64bits(data, length);
    uint32_t index = hash % repo.capacity;
    uint32_t start = index;

    do {
        if (!repo.entries[index].data) return 0;

        if (repo.hash_values[index] == hash &&
            repo.entries[index].length == length &&
            memcmp(repo.entries[index].data, data, length) == 0) {
            if (repo.values[index] > 0)
                repo.values[index]--;
            return repo.values[index];
        }

        index = (index + 1) % repo.capacity;
    } while (index != start);

    return 0;
}
