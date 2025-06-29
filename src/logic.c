#include "logic.h"
#include "graph/graph.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "graph/graph_visualizer.h"
#include "graph/shortest_path.h"
#include "map/sequence_repository_useless.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

static GraphNode* root_node = NULL;

static inline void create_root() {   
    increment_graph_level();
    root_node = get_next_node();
    assert(root_node != NULL);
}

static GraphNode* create_node(uint32_t start, uint8_t length) {
    GraphNode* node = get_next_node();    
    if (!node) return NULL;
    
    node->offset = start;
    node->sequence_length = length;
    return node;
}


void print_shortest_path() {
    uint32_t *path = NULL;
    uint32_t path_length = 0;
    int cost = -1;
    find_shortest_path_to_sink();

    if (cost != -1) {
        printf("Shortest path cost: %d\n", cost);
        printf("Path: ");
        for (uint32_t i = 0; i < path_length; i++) {
            printf("%u", path[i]);
            if (i < path_length - 1)
                printf(" -> ");
        }
        printf("\n");
        free(path);
    } else {
        printf("No path found to sink\n");
    }
}

void process_block(const uint8_t *block, uint32_t block_size) {
    init_graph();
    create_root();
    seq_repo_init();

#ifdef DEBUG
    print_graph_node(get_graph_node(0));
#endif

    for (uint32_t block_index = 0; block_index < block_size; block_index++) {
        increment_graph_level();
        uint16_t current_level = get_last_level_index();
        if (current_level >= MAX_LEVELS)
            break;

        uint8_t max_sequence = MIN(current_level, MAX_WEIGHTS);

        // Prevent underflow: valid sequences must start within bounds
        for (uint8_t seq_len = 1; seq_len <=  max_sequence; seq_len++) {
            uint32_t start = block_index - seq_len + 1;
            GraphNode *node = create_node(start, seq_len);

            if (seq_len > 1) {
                uint32_t node_id = seq_repo_get_node_id(&block[start], seq_len);
                node->isUseless = 1; // Assume useless initially

                if (node_id != UINT32_MAX) {
                    GraphNode *g_node = get_graph_node(node_id);
                    if (g_node) g_node->isUseless = 0; // Mark existing node as useful
                    node->isUseless = 0;                // Mark current node as useful
                } else {
                    seq_repo_add(&block[start], seq_len, node->node_id);
                    // Still marked useless until repeated
                }
            } else {
                // 1-length sequences are always useful and not tracked
                node->isUseless = 0;
            }

#ifdef DEBUG
            print_graph_node(node);
#endif
        }
    }
    find_shortest_path_to_sink();
#ifdef DEBUG
    visualize_graph(block);
#endif
}
