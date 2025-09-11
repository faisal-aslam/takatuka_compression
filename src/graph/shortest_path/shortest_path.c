#include "shortest_path.h"
#include "seq_freq_map.h"

/**
 * Prints either the current path or the best path.
 * @param isCurrent If true, prints current path; otherwise prints best path
 * @param shouldPrintData If true, prints sequence details as well
 * @param block Pointer to input block (for sequence data)
 */
void print_path(uint8_t isCurrent, uint8_t shouldPrintData, const uint8_t *block, Path *path_state) {
    const int idx = isCurrent ? PATH_CURRENT : PATH_BEST;
    const int size = path_state->path_size[idx];

    if (size < 0) return; // No path

    CHECK_INDEX(size, "print_path");

    uint32_t total_saving = path_state->path_total_saving[idx];
    const uint32_t *stack = path_state->path_stack[idx];
    const uint32_t *freqs = path_state->path_freqs[idx];
    const uint32_t *per_node_savings = path_state->path_per_node_savings[idx];

    printf("\n=== %s PATH ===\n", isCurrent ? "CURRENT" : "BEST");
    printf("Path size = %d, Total saving = %u, Total cost=%u\n", size + 1, total_saving,
           path_state->path_total_cost[idx]);
    printf("Node chain (node_id, level):\n");

    for (int i = size; i >= 0; i--) {
        GraphNode *node = get_graph_node(stack[i]);
        if (!node) continue;
        printf("(%u,%u)", node->node_id, node->node_level);
        if (i > 0) printf(" -> ");
    }
    printf("\n");

    if (!shouldPrintData) return;

    printf("\nDetailed sequence info:\n");
    for (int i = size; i >= 0; i--) {
        GraphNode *node = get_graph_node(stack[i]);
        if (!node) continue;

        uint8_t len = node->sequence_length;
        uint32_t freq = freqs[i];
        uint32_t saving = per_node_savings[i];
        uint32_t cost_per_node = path_state->path_per_node_cost[idx][i];

        printf("\n -> ");
        if (node->RLE_type) {
            printf("RLE=YES ");
        }

        printf("| id=%u len=%u freq=%u saving=%u cost=%u | ", node->node_id, len, freq, saving, cost_per_node);

        print_node_sequence(node, block);

        if (i % 20 == 0) fflush(stdout);
    }

    printf("\n\n");
}


void final_book_keeping(const uint8_t *block, Path *path_state) {
    init_seq_freq_map();
    GraphNode *node;
    const uint32_t *path = path_state->path_stack[PATH_BEST];
    int32_t path_len = path_state->path_size[PATH_BEST];
    if (path_len <= 0) return;

    // Pass 1: Count sequence frequencies
    for (int32_t i = 0; i < path_len; i++) {
        node = get_graph_node(path[i]);
        if (node->sequence_length > 1 && !node->RLE_type) {
            seq_freq_increment(&block[node->offset], node->sequence_length, node->node_id);
        }
    }

    // Pass 2: Store frequencies per node
    for (int32_t i = 0; i < path_len; i++) {
        node = get_graph_node(path[i]);
        if (node->sequence_length > 1 && !node->RLE_type) {
            uint32_t freq, node_id_unused;
            seq_freq_get(&block[node->offset], node->sequence_length, &freq, &node_id_unused);
            path_state->path_freqs[PATH_BEST][i] = freq;
        } else {
            path_state->path_freqs[PATH_BEST][i] = 1; // default for single-byte or RLE
        }
    }
}