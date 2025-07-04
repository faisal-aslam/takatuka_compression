
#include "sequence_repository_useless.h"
#include "sequence_repository_freq.h"
#include "../constants.h"

uint32_t hash_index_cache[MAX_GRAPH_NODES]; //for frequency hash (not for binary hash)
void seq_repo_init(SequenceRepository *repo, uint32_t inital_capcity) {
    printf("\n creating map of capacity=%u\n", inital_capcity);
    if (inital_capcity == 0 || inital_capcity > INITIAL_CAPACITY) {
        repo->capacity = INITIAL_CAPACITY;
    } else {
        repo->capacity = inital_capcity;
    }
    repo->count = 0;
    repo->entries = calloc(repo->capacity, sizeof(SequenceEntry));
    repo->hash_values = calloc(repo->capacity, sizeof(uint64_t));
    repo->values = calloc(repo->capacity, sizeof(uint32_t));
    repo->is_used = calloc(repo->capacity, sizeof(uint8_t));
    assert(repo->entries && repo->values && repo->hash_values && repo->is_used);
}

void seq_repo_cleanup(SequenceRepository *repo) {
    free(repo->entries);
    free(repo->hash_values);
    free(repo->values);
    free(repo->is_used);
    memset(repo, 0, sizeof(SequenceRepository));
}

void seq_repo_reset(SequenceRepository *repo) {
    memset(repo->entries, 0, sizeof(SequenceEntry) * repo->capacity);
    memset(repo->hash_values, 0, sizeof(uint64_t) * repo->capacity);
    memset(repo->values, 0, sizeof(uint32_t) * repo->capacity);
    memset(repo->is_used, 0, sizeof(uint8_t) * repo->capacity);
    repo->count = 0;

    for (uint32_t i = 0; i < MAX_GRAPH_NODES; i++) {
        hash_index_cache[i] = UINT32_MAX;
    }
}

static void resize_repository(SequenceRepository *repo) {
    uint32_t old_capacity = repo->capacity;
    SequenceEntry* old_entries = repo->entries;
    uint64_t* old_hash_values = repo->hash_values;
    uint32_t* old_values = repo->values;
    uint8_t* old_is_used = repo->is_used;

    repo->capacity *= GROWTH_FACTOR;
    repo->count = 0;
    repo->entries = calloc(repo->capacity, sizeof(SequenceEntry));
    repo->hash_values = calloc(repo->capacity, sizeof(uint64_t));
    repo->values = calloc(repo->capacity, sizeof(uint32_t));
    repo->is_used = calloc(repo->capacity, sizeof(uint8_t));
    assert(repo->entries && repo->hash_values && repo->values && repo->is_used);

    for (uint32_t i = 0; i < old_capacity; i++) {
        if (old_is_used[i]) {
            const uint8_t* data = old_entries[i].data;
            uint16_t len = old_entries[i].length;
            uint64_t hash = old_hash_values[i];
            uint32_t index = hash % repo->capacity;

            while (repo->is_used[index]) {
                index = (index + 1) % repo->capacity;
            }
            repo->entries[index].data = data;
            repo->entries[index].length = len;
            repo->hash_values[index] = hash;
            repo->values[index] = old_values[i];
            repo->is_used[index] = 1;
            repo->count++;
        }
    }

    free(old_entries);
    free(old_hash_values);
    free(old_values);
    free(old_is_used);

    for (uint32_t i = 0; i < MAX_GRAPH_NODES; i++) {       
        hash_index_cache[i] = UINT32_MAX;
    }

}

void seq_repo_add(SequenceRepository *repo, const uint8_t* data, uint16_t length, uint32_t node_id) {
    if (!data || length <= 1) return;
    if (repo->count >= repo->capacity * LOAD_FACTOR) resize_repository(repo);

    uint64_t hash = XXH3_64bits(data, length);
    uint32_t index = hash % repo->capacity;
    uint32_t start = index;

    do {
        if (!repo->is_used[index]) {
            repo->entries[index].data = data;
            repo->entries[index].length = length;
            repo->hash_values[index] = hash;
            repo->values[index] = node_id;
            repo->is_used[index] = 1;
            repo->count++;
            return;
        }
        if (repo->hash_values[index] == hash &&
            repo->entries[index].length == length &&
            memcmp(repo->entries[index].data, data, length) == 0) {
            if (repo->values[index] != node_id) {
                repo->values[index] = UINT32_MAX;
            }
            return;
        }
        index = (index + 1) % repo->capacity;
    } while (index != start);
}

uint32_t seq_repo_get_node_id(SequenceRepository *repo, const uint8_t* data, uint16_t length) {
    if (!data || length <= 1) return UINT32_MAX;
    uint64_t hash = XXH3_64bits(data, length);
    uint32_t index = hash % repo->capacity;
    uint32_t start = index;

    do {
        if (!repo->is_used[index]) return UINT32_MAX;
        if (repo->hash_values[index] == hash &&
            repo->entries[index].length == length &&
            memcmp(repo->entries[index].data, data, length) == 0) {
            return repo->values[index];
        }
        index = (index + 1) % repo->capacity;
    } while (index != start);

    return UINT32_MAX;
}

uint32_t seq_repo_increase_frequency(SequenceRepository *repo, const uint8_t* data, uint16_t length) {
    if (!data || length <= 1) return 0;
    if (repo->count >= repo->capacity * LOAD_FACTOR) {
        resize_repository(repo);    
    }

    uint64_t hash = XXH3_64bits(data, length);
    uint32_t start = hash % repo->capacity;
    uint32_t index = start;

    do {
        if (!repo->is_used[index]) {
            repo->entries[index].data = data;
            repo->entries[index].length = length;
            repo->hash_values[index] = hash;
            repo->values[index] = 1;
            repo->is_used[index] = 1;
            repo->count++;
            return 1;
        }
        if (repo->hash_values[index] == hash &&
            repo->entries[index].length == length &&
            memcmp(repo->entries[index].data, data, length) == 0) {
            assert(repo->values[index] < UINT32_MAX);
            return ++repo->values[index];
        }
        index = (index + 1) % repo->capacity;
    } while (index != start);

    return 0;
}


uint32_t seq_repo_get_frequency(SequenceRepository *repo, const uint8_t* data, uint16_t length) {
    if (!data || length <= 1) return 0;

    uint64_t hash = XXH3_64bits(data, length);
    uint32_t index = hash % repo->capacity;
    uint32_t start = index;

    do {
        if (!repo->is_used[index]) {
            // Not found
            return 0;
        }

        if (repo->hash_values[index] == hash &&
            repo->entries[index].length == length &&
            memcmp(repo->entries[index].data, data, length) == 0) {
            return repo->values[index]; // Found
        }

        index = (index + 1) % repo->capacity;
    } while (index != start);

    return 0; // Not found after full loop
}

uint32_t seq_repo_increase_frequency_cached(SequenceRepository *repo, const uint8_t* data, uint16_t length, uint32_t node_id) {
    uint32_t index = hash_index_cache[node_id];
    if (!data || length <= 1) return 0;
    if (repo->count >= repo->capacity * LOAD_FACTOR) {
        resize_repository(repo);        
    }
    //fast using index.
    if (index < repo->capacity && repo->is_used[index]) {
        SequenceEntry* e = &repo->entries[index];
        if (e->length == length && e->data == data) {
            assert(repo->values[index] < UINT32_MAX);
            return ++repo->values[index];
        }
    }

    uint64_t hash = XXH3_64bits(data, length);
    uint32_t start = hash % repo->capacity;
    index = start;

    do {
        if (!repo->is_used[index]) {
            repo->entries[index].data = data;
            repo->entries[index].length = length;
            repo->hash_values[index] = hash;
            repo->values[index] = 1;
            repo->is_used[index] = 1;
            hash_index_cache[node_id] = index; //save index the first time.
            repo->count++;
            return 1;
        }
        if (repo->hash_values[index] == hash &&
            repo->entries[index].length == length &&
            memcmp(repo->entries[index].data, data, length) == 0) {
            assert(repo->values[index] < UINT32_MAX);
            hash_index_cache[node_id] = index; //save index
            return ++repo->values[index];
        }
        index = (index + 1) % repo->capacity;
    } while (index != start);

    return 0;
}

uint32_t seq_repo_decrease_by_index(SequenceRepository *repo, uint32_t node_id) {
    uint32_t index = hash_index_cache[node_id]; //get the saved index by node_id.
    if (index >= repo->capacity || !repo->is_used[index]) return 0;
    if (repo->values[index] > 0)
        repo->values[index]--;
    return repo->values[index];
}


void seq_repo_print_all(SequenceRepository *repo) {
    if (!repo) {
        printf("Repository is NULL\n");
        return;
    }

    printf("Repository contents (count=%u):\n", repo->count);
    for (uint32_t i = 0; i < repo->capacity; i++) {
        if (repo->is_used[i]) {
            SequenceEntry *entry = &repo->entries[i];
            printf(" [%3u] Len=%u, Freq=%u, Seq=", i, entry->length, repo->values[i]);
            for (uint8_t j = 0; j < entry->length; j++) {
                printf("%02X", entry->data[j]);
                if (j + 1 < entry->length) {
                    printf(",");
                }
            }
            printf("\n");
        }
    }
}
