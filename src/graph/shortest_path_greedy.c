// shortest_path.c

#include "seq_freq_map.h"
#include "shortest_path.h"
#include <float.h>
#include <math.h>
#include <stdbool.h>

int prune_count = 0;

#define CHECK_INDEX(idx, label)                                                                                        \
    if ((idx) < 0 || (idx) >= MAX_LEVELS) {                                                                            \
        fprintf(stderr, "ERROR: Index %d out of bounds in %s (MAX_LEVELS = %d)\n", (idx), (label), MAX_LEVELS);        \
        abort();                                                                                                       \
    }

typedef struct {
    uint32_t node_id;
    uint32_t node_id_popped;
    uint32_t hash_index_cache;
} StackItem;

Path path_state;

/**
 * @brief Calculates the storage saving
 *
 * @param node Pointer to graph node being evaluated
 * @param frequency Frequency count of this sequence in the data
 * @return double Storage saving in bytes
 */
static inline double calc_savings(GraphNode *node, uint32_t frequency) {

    // Branchless design for common cases - reduces pipeline stalls
    const uint8_t len = node->sequence_length;
    double base_saving;

    if (node->node_id == 0) return 0; // no savings for the root node.

    // Handle RLE case first (uses different saving model)
    if (node->is_RLE) {
        // RLE saving: pattern length + 1 byte for repeat count
        double ret = (node->length_of_RLE / node->repeat_seq_length);
        return (ret * ret * ret);
    }

    // Main saving calculation branches
    if (len <= 1) {
        // Cases: 0 bytes = 0/1 saving, 0
        base_saving = 0;
    } else {
        // Multi-byte case: savings is based on length and frequency.
        base_saving = (frequency - 1) * node->sequence_length * sqrt((double)node->sequence_length);
    }

    return base_saving;
}

/**
 * Initializes the path state for a new search.
 */
static inline void path_init() {
    memset(&path_state, 0, sizeof(Path));
    path_state.path_size[PATH_CURRENT] = -1;
    path_state.path_size[PATH_BEST] = -1;
    path_state.path_total_saving[PATH_CURRENT] = 0;
    path_state.path_total_saving[PATH_BEST] = -1;
    path_state.path_total_freq[PATH_BEST] = 0;
    path_state.path_total_freq[PATH_CURRENT] = 0;
}

/**
 * Prints either the current path or the best path.
 * @param isCurrent If true, prints current path; otherwise prints best path
 * @param shouldPrintData If true, prints sequence details as well
 * @param block Pointer to input block (for sequence data)
 */
void print_path(uint8_t isCurrent, uint8_t shouldPrintData, const uint8_t *block) {
    const int idx = isCurrent ? PATH_CURRENT : PATH_BEST;
    const int32_t size = path_state.path_size[idx];
    if (size < 0) return; // no path exist.
    CHECK_INDEX(size - 1, "print_path");

    const double total_saving = path_state.path_total_saving[idx];
    const uint32_t *stack = path_state.path_stack[idx];
    const uint32_t *freqs = path_state.path_freqs[idx];
    const double *per_node_savings = path_state.path_per_node_savings[idx];

    printf("\n=== %s PATH ===\n", isCurrent ? "CURRENT" : "BEST");
    printf("Path size = %d, Total saving = %.2lf, Total freq=%u \n", size + 1, total_saving,
           path_state.path_total_freq[idx]);
    printf("Node chain (node_id, level):\n");

    for (int32_t i = size; i >= 0; i--) {
        GraphNode *node = get_graph_node(stack[i]);
        if (!node) continue;
        printf("(%u,%u)", node->node_id, node->node_level);
        if (i > 0) printf(" -> ");
    }
    printf("\n");

    if (!shouldPrintData) return;

    printf("\nDetailed sequence info:\n");
    for (int32_t i = size; i >= 0; i--) {
        GraphNode *node = get_graph_node(stack[i]);
        if (!node) continue;

        const uint8_t len = node->sequence_length;
        const uint32_t freq = freqs[i];
        const double saving = per_node_savings[i];

        printf("\n -> ");
        if (node->is_RLE) {
            printf("RLE=YES ");
        }

        printf("| id=%u len=%u freq=%u saving=%.2f | ", node->node_id, len, freq, saving);

        print_node_sequence(node, block);

        if (i % 20 == 0) fflush(stdout);
    }

    printf("\n\n");
}

/*
            if (node->sequence_length > 1 && !node->is_RLE) {
                uint32_t freq, node_id;
                uint32_t index = seq_freq_get_with_index(&block[node->offset], node->sequence_length, &freq, &node_id);
                if (freq > 1) {
                    seq_freq_set_existing(index, freq - 1, node_id);
                }
            }

*/

static inline void get_max_saving_node_of_level(uint16_t level, const uint8_t *block, double *node_savings,
                                                uint32_t *node_freq, uint32_t *max_savings_node_id) {

    uint32_t start_id = get_level_start_id(level);
    uint32_t end_id = get_level_end_id(level);
    *node_savings = -1;
    *max_savings_node_id = UINT32_MAX;
    for (uint32_t id = start_id; id < end_id; id++) {
        GraphNode *node = get_graph_node(id);
        if (node->useless) continue;
        if (node->node_level != level) {
            fprintf(stderr, "Illegal level \n");
            abort();
        }
        uint32_t node_id;
        seq_freq_get(&block[node->offset], node->sequence_length, node_freq, &node_id);
        double savings = calc_savings(node, *node_freq);
        if (savings > *node_savings) {
            *max_savings_node_id = node->node_id;
            *node_savings = savings;
        }
    }
    if (*max_savings_node_id == UINT32_MAX) {
        fprintf(stderr, "Illegal level %u \n", level);
        abort();
    }
}

void find_best_saving_path(const uint8_t *block, uint16_t starting_level) {

    path_init(); // Reset path state
    while (1) {
        double node_savings;
        uint32_t freq, node_id;
        get_max_saving_node_of_level(starting_level, block, &node_savings, &freq, &node_id);
        GraphNode *node = get_graph_node(node_id);
        path_state.path_total_saving[PATH_BEST] += node_savings;
        path_state.path_size[PATH_BEST]++;
        path_state.path_total_freq[PATH_BEST] += freq;
        path_state.path_per_node_savings[PATH_BEST][path_state.path_size[PATH_BEST]] = node_savings;
        path_state.path_stack[PATH_BEST][path_state.path_size[PATH_BEST]] = node_id;
        if (node->sequence_length > 1 &&
            !node->is_RLE) { // so that other nodes also select this node with higher probability.
            uint32_t index = seq_freq_get_with_index(&block[node->offset], node->sequence_length, &freq, &node_id);
            seq_freq_set_existing(index, freq + 3, node_id);
        }
        if (node->node_id == 0) break;
        starting_level = get_parent_level(node);
    }
}

void final_book_keeping(const uint8_t *block) {
    init_seq_freq_map();
    GraphNode *node;
    const uint32_t *path = path_state.path_stack[PATH_BEST];
    uint32_t path_len = path_state.path_size[PATH_BEST];

    // Pass 1: Count sequence frequencies
    for (uint32_t i = 0; i <= path_len; i++) {
        node = get_graph_node(path[i]);
        if (node->sequence_length > 1 && !node->is_RLE) {
            seq_freq_increment(&block[node->offset], node->sequence_length, node->node_id);
        }
    }

    // Pass 2: Store frequencies per node
    for (uint32_t i = 0; i <= path_len; i++) {
        node = get_graph_node(path[i]);
        if (node->sequence_length > 1 && !node->is_RLE) {
            uint32_t freq, node_id_unused;
            seq_freq_get(&block[node->offset], node->sequence_length, &freq, &node_id_unused);
            path_state.path_freqs[PATH_BEST][i] = freq;
        } else {
            path_state.path_freqs[PATH_BEST][i] = 1; // or other sentinel if needed
        }
    }
}

void free_path_state() {
    // Only if path_state has dynamic allocations
    memset(&path_state, 0, sizeof(Path));
}