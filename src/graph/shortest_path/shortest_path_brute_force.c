// shortest_path.c

#include "shortest_path_brute_force.h"
#include <math.h>
#include <stdbool.h>

int prune_count = 0;


/**
 * Performs a DFS-based traversal (using a manual stack to avoid recursion)
 * to find the best-saving path from any leaf node to the root node (node_id
 * == 0) in a DAG (Directed Acyclic Graph) representing a compression graph.
 *
 * The path saving is computed based on the frequency and length of sequences at
 * each node using calc_saving function.
 */
void find_best_saving_path(const uint8_t *block, Path* path_state) {
    init_seq_freq_map();// initialize the sequence map.
    find_best_saving_path_to_a_node(block, get_last_level_index(), 0, path_state);    
}
