#include "best_path_view.h"
#include <assert.h>

BestPathView view;


/**
 * @brief Sets the global BestPathView used by compression.
 *
 * Every shortest path algorithm variant must call this function after it finishes.
 * The compression stage relies on the view set here to write the compressed output.
 *
 * Important:
 *  - The arrays passed (path_ids, path_frequencies) must remain valid for the lifetime
 *    of 'view' (e.g. allocated on the heap or managed globally).
 *    Do NOT pass pointers to temporary stack arrays.
 *  - path_size must be non-negative and reflect the number of nodes in the path.
 */
void set_best_path_view(uint32_t *path_ids, uint32_t *path_frequencies, int32_t path_size) {
    // Defensive checks
    assert(path_ids != NULL && "path_ids must not be NULL");
    assert(path_frequencies != NULL && "path_frequencies must not be NULL");
    assert(path_size >= 0 && "path_size must not be negative");

    view.nodes = path_ids;
    view.freqs = path_frequencies;
    view.path_size = path_size;
}



/**
 * Prints the best path from a BestPathView structure
 * @param view Pointer to BestPathView structure
 * @param shouldPrintData If true, prints sequence details as well
 * @param block Pointer to input block (for sequence data)
 */
void print_best_view(const BestPathView *view, uint8_t shouldPrintData, const uint8_t *block) {
    printf("\n=== BEST PATH VIEW ===\n");
    printf("Path size = %d\n", view->path_size);
    printf("Node chain (node_id, level):\n");

    // Print node chain (reverse order as in original)
    for (int32_t i = view->path_size - 1; i >= 0; i--) {
        GraphNode *node = get_graph_node(view->nodes[i]);
        if (!node) continue;
        printf("(%u,%u)", node->node_id, node->node_level);
        if (i > 0) printf(" -> ");
    }
    printf("\n");

    if (!shouldPrintData) return;

    printf("\nDetailed sequence info:\n");
    for (int32_t i = view->path_size - 1; i >= 0; i--) {
        GraphNode *node = get_graph_node(view->nodes[i]);
        if (!node) continue;

        const uint8_t len = node->sequence_length;
        const uint32_t freq = view->freqs[i];

        printf("\n -> ");
        if (node->is_RLE) {
            printf("RLE=YES ");
        }

        printf("| id=%u len=%u freq=%u | ",
               node->node_id, len, freq);

        print_node_sequence(node, block);

        if (i % 20 == 0) fflush(stdout);
    }
    printf("\n\n");
}

void free_best_path_view(BestPathView* view) {
    memset(view, 0, sizeof(BestPathView)); // Optional safety
}