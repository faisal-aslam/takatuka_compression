#include "best_path_view.h"
#include "shortest_path.h"




BestPathView get_best_path_view() {
    const int idx = PATH_BEST;

    return (BestPathView){
        // Direct pointers to existing arrays
        .nodes = path_state.path_stack[idx],
        .freqs = path_state.path_freqs[idx],        
        // Metadata
        .path_size = path_state.path_size[idx] + 1, // Convert to count        
    };
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

        printf(" -> ");
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