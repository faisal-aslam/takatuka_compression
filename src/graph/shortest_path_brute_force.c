// shortest_path.c

#include "shortest_path_brute_force.h"
#include <math.h>
#include <stdbool.h>

int prune_count = 0;

Path path_state;
StackItem main_stack[MAX_GRAPH_NODES * 2 + 1];
uint32_t max_saving_node_ids[MAX_LEVELS]; // Best immediate-savings node per level
uint32_t best_savings_node_ids[MAX_LEVELS];

/**
 * Performs a DFS-based traversal (using a manual stack to avoid recursion)
 * to find the best-saving path from any leaf node to the root node (node_id
 * == 0) in a DAG (Directed Acyclic Graph) representing a compression graph.
 *
 * The path saving is computed based on the frequency and length of sequences at
 * each node using calc_saving function.
 */
void find_best_saving_path(const uint8_t *block, uint16_t starting_level) {

    int top = -1;

    uint32_t back_track_count = 0;
    uint32_t best_count = 0;
    uint32_t push_count = 0;
    long max_push = get_graph_size()*get_graph_size();

    path_init();         // Reset path state
    init_seq_freq_map(); // initalize the seqeunce map.

    initialize_leaf_nodes(main_stack, &top, starting_level);
    if (top < 0) {
        printf("empty level\n");
        return; // no path exist.
    }
    while (top >= 0) {
        if (push_count > max_push && best_count >= 1) {
            //break;            
        }
        StackItem current = main_stack[top--];
#ifdef DEBUG
        printf("pop stack node_id=%u, from top=%d, node_id_popped=%u\n", current.node_id, top + 1,
               current.node_id_popped);
#endif

        if (current.node_id == UINT32_MAX) {
            // Backtrack marker encountered
            backtrack_node(block);
            back_track_count++;
            continue;
        }
        GraphNode *node = get_graph_node(current.node_id);

        // The following pruning is very useful for speed up.
        // In it, we do not explore paths which are worse.
        // Early pruning before frequency saving and stack updates
        if (should_prune(node)) {
            continue;
        }

        process_node(block, node);
        push_count++;

        // Push backtrack marker
        main_stack[++top] = (StackItem){.node_id = UINT32_MAX, .node_id_popped = current.node_id};
#ifdef DEBUG
        printf("Push in stack position %d backrack for node=%u\n", top, node->node_id);
#endif

        // If reached root node (ID 0), check and update best path
        if (current.node_id == 0) {

#ifdef DEBUG
            print_path(1, 1, block);
#endif
            if (update_best_path()) {
                best_count++;
#ifdef DEBUG
                printf("Saved the path %d with saving: %u\n", best_count, path_state.path_total_saving[PATH_CURRENT]);
                printf("\nbest_count=%u, prune_count=%u, back_track_count=%u, push_count=%u\n", best_count, prune_count,
                       back_track_count, push_count);
                print_path(0, 1, block);
#endif
                push_count = 0;
                back_track_count = 0;
                prune_count = 0;
            }
            continue; // Root has no parents
        }

        // Explore parents
        add_parent_nodes_to_stack(main_stack, &top, node, block);
    }
    // Final output
#ifdef DEBUG
    printf("\nbest_count=%u, prune_count=%u, back_track_count=%u, push_count=%u\n", best_count, prune_count,
           back_track_count, push_count);
    print_path(0, 1, block);
#endif
    bookkeeping_best_path(starting_level, block);

    // free_path_state();
}
