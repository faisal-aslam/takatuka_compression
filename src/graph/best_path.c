//best_path.c

// Best Path Cost Calculation (Path-Aware Global Hashmap Version)
/*
This section calculates the cost of each link in the graph by traversing all paths
from last-level nodes to the root, using a shared hashmap that tracks sequence frequencies
**per originating path** to distinguish between different contexts.

Core Idea:
- A global hashmap is used, but each entry contains a list of frequencies indexed by `path_id`,
  which represents the ID of the originating last-level node of the current traversal.

Procedure:
1. Assign each last-level node a unique `path_id` and begin traversing upward toward the root.

2. For each node during traversal:
   a. If its sequence length > 1:
      - Update the global hashmap with this sequence.
      - In the hashmap entry, either:
        • Create a new (path_id, frequency=1) entry, or
        • Increment frequency if (path_id) already exists.

   b. For each incoming link to the node:
      - Retrieve the frequency of this node’s sequence for the current `path_id`.
      - If frequency == 1:
          → Set `link_cost = sequence_length + 1`
        Else:
          → Set `link_cost = 1`

3. Continue this process until reaching the root for all paths.

4. After traversal, the cost of all links is computed with full awareness of per-path sequence reuse.
   - The best path is defined as the one with the lowest total link cost from a last-level node to the root.

Advantages:
- Avoids per-path hashmap duplication.
- Ensures correctness by isolating frequency counts per path via `path_id`.
- Efficient memory usage with compact frequency lists.

*/

#include "graph.h"
#include "../map/sequence_repository.h"
#include "best_path.h"
#include "../map/xxhash.h"

#define MAX_PATHS 512  // Max unique paths from last-level nodes
#define HASH_BUCKETS 1024

// Frequency entry for a specific path
typedef struct FrequencyEntry {
    uint32_t path_id;
    uint16_t frequency;
    struct FrequencyEntry* next;
} FrequencyEntry;

// Hash table entry for a sequence
typedef struct {
    const uint8_t* sequence;
    uint16_t length;
    FrequencyEntry* freq_list;
    bool used;
} PathAwareEntry;

// Hashmap for tracking frequencies per path
static PathAwareEntry frequency_table[HASH_BUCKETS];

// Helper: Compare sequences
static bool seq_equal(const uint8_t* a, const uint8_t* b, uint16_t len) {
    for (int i = 0; i < len; ++i) if (a[i] != b[i]) return false;
    return true;
}

// Hash function wrapper
static inline uint64_t hash_sequence(const uint8_t* seq, uint16_t len) {
    return XXH3_64bits(seq, len) % HASH_BUCKETS;
}

// Insert or increment sequence frequency for a path
static void increment_path_frequency(uint32_t path_id, const uint8_t* seq, uint16_t len) {
    uint64_t hash = hash_sequence(seq, len);
    for (uint64_t i = 0; i < HASH_BUCKETS; ++i) {
        uint64_t idx = (hash + i) % HASH_BUCKETS;
        if (!frequency_table[idx].used) {
            frequency_table[idx].sequence = seq;
            frequency_table[idx].length = len;
            frequency_table[idx].used = true;
            FrequencyEntry* fe = malloc(sizeof(FrequencyEntry));
            fe->path_id = path_id;
            fe->frequency = 1;
            fe->next = NULL;
            frequency_table[idx].freq_list = fe;
            return;
        } else if (frequency_table[idx].length == len &&
                   seq_equal(frequency_table[idx].sequence, seq, len)) {
            FrequencyEntry* current = frequency_table[idx].freq_list;
            while (current) {
                if (current->path_id == path_id) {
                    current->frequency++;
                    return;
                }
                current = current->next;
            }
            // Not found, insert new frequency entry
            FrequencyEntry* fe = malloc(sizeof(FrequencyEntry));
            fe->path_id = path_id;
            fe->frequency = 1;
            fe->next = frequency_table[idx].freq_list;
            frequency_table[idx].freq_list = fe;
            return;
        }
    }
}

// Get frequency of a sequence for a path
static uint16_t get_frequency(uint32_t path_id, const uint8_t* seq, uint16_t len) {
    uint64_t hash = hash_sequence(seq, len);
    for (uint64_t i = 0; i < HASH_BUCKETS; ++i) {
        uint64_t idx = (hash + i) % HASH_BUCKETS;
        if (!frequency_table[idx].used) return 0;
        if (frequency_table[idx].length == len &&
            seq_equal(frequency_table[idx].sequence, seq, len)) {
            FrequencyEntry* current = frequency_table[idx].freq_list;
            while (current) {
                if (current->path_id == path_id) return current->frequency;
                current = current->next;
            }
            return 0;
        }
    }
    return 0;
}

// Free the entire hashmap
static void clear_frequency_table(void) {
    for (int i = 0; i < HASH_BUCKETS; ++i) {
        if (!frequency_table[i].used) continue;
        FrequencyEntry* cur = frequency_table[i].freq_list;
        while (cur) {
            FrequencyEntry* tmp = cur;
            cur = cur->next;
            free(tmp);
        }
        frequency_table[i].used = false;
        frequency_table[i].freq_list = NULL;
        frequency_table[i].sequence = NULL;
        frequency_table[i].length = 0;
    }
}

static void compute_costs_from_node(GraphNode* node, uint32_t path_id, const uint8_t* block) {
    if (!node) return;

    // Step 1: Update frequency table
    if (node->compress_sequence_length > 1) {
        increment_path_frequency(path_id, &block[node->compress_start_index], node->compress_sequence_length);
    }

    // Step 2: Update cost for each incoming link
    for (uint8_t i = 0; i < node->parent_link_count; ++i) {
        ParentLink* link = &node->parent_links[i];
        GraphNode* parent = graph_get_node(link->parent_node_id);

        uint16_t freq = get_frequency(path_id, &block[node->compress_start_index], node->compress_sequence_length);
        if (freq == 1) {
            link->link_cost = node->compress_sequence_length + 1;
        } else {
            link->link_cost = 1;
        }

        compute_costs_from_node(parent, path_id, block);
    }
}

void find_and_print_best_path(const uint8_t* block) {
    uint32_t path_id = 0;
    uint32_t max_level = get_max_level();

    for (uint8_t weight = 0; weight < SEQ_LENGTH_LIMIT; ++weight) {
        uint32_t count = 0;
        const uint32_t* indices = get_nodes_by_weight_and_level(weight, max_level, &count);
        for (uint32_t i = 0; i < count; ++i) {
            clear_frequency_table();
            GraphNode* node = graph_get_node(indices[i]);
            compute_costs_from_node(node, path_id++, block);
        }
    }

    // TODO: Evaluate link_cost sums and select best path
    printf("[Info] Cost calculation complete. Best path selection not yet implemented.\n");
}
