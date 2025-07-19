// shortest_path.c

#include "shortest_path.h"
#include "seq_freq_map.h"
#include <float.h>

int prune_count = 0;

typedef struct {
    uint32_t node_id;
    uint32_t node_id_popped;
    uint32_t hash_index_cache;
} StackItem;

Path path_state;

static inline double calc_savings(GraphNode *node, uint32_t frequency) {    
    return node->sequence_length*frequency;
}

/**
 * @brief Calculates the storage cost in bytes for adding a graph node to a path
 *
 * This is a hot path function - optimized for minimal branching and fast execution.
 * All costs are calculated in bytes of storage required.
 *
 * Cost Rules:
 * - Zero-length sequences: 0 bytes (invalid case, handled defensively)
 * - Single-byte sequences: 1 byte (raw byte)
 * - Multi-byte unique sequences (freq=1): n+1 bytes (n bytes + 1 byte length prefix)
 * - Multi-byte repeated sequences (freq>1): 1 byte (reference to dictionary)
 * - RLE sequences: pattern_length + 1 byte (pattern + repeat count)
 *
 * @param node Pointer to graph node being evaluated
 * @param frequency Frequency count of this sequence in the data
 * @return double Storage cost in bytes (always >= 0)
 */
static inline double calc_cost(GraphNode *node, uint32_t frequency) {
    
    // Branchless design for common cases - reduces pipeline stalls
    const uint8_t len = node->sequence_length;
    double base_cost;

    if (node->node_id == 0) return 0; // no cost for the root node.

    // Handle RLE case first (uses different cost model)
    if (node->is_RLE) {
        // RLE cost: pattern length + 1 byte for repeat count
        return node->repeat_seq_length + 1;
    }

    // Main cost calculation branches
    if (len <= 1) {
        // Cases: 0 bytes = 0 cost, 1 byte = 1 cost
        base_cost = len;
    } else {
        // Multi-byte case: 1 byte if repeated, n+1 bytes if unique
        base_cost = (frequency > 1) ? 1 : (len + 1);
    }

// Debug verification (compiled out in release builds)
#ifdef DEBUG
    if (base_cost == 0 && len != 0) {
        printf("WARNING: Zero cost for non-zero length node %u\n", node->node_id);
    }
#endif

    return base_cost;
}

/**
 * @brief Finds the node with the minimum cost at a given level.
 * 
 * @param level The level to search for the minimum cost node.
 * @param block Pointer to the input data block (used to extract sequence).
 * @return GraphNode* Pointer to the node with the minimum cost, or NULL if level is invalid or empty.
 */
GraphNode* find_min_cost_node_at_level(uint16_t level, const uint8_t* block) {
    if (level >= graph.total_levels) {
        fprintf(stderr, "Invalid level: %u (max: %u)\n", level, graph.total_levels);
        return NULL;
    }
    if (level == 0) return get_graph_node(0);
    uint32_t start = get_level_start_id(level);
    uint32_t end = get_level_end_id(level);

    GraphNode* best_node = NULL;
    double max_savings = DBL_MIN;

    for (uint32_t i = start; i < end; i++) {
        GraphNode* node = get_graph_node(i);
        const uint8_t* seq = block + node->offset;
        uint8_t len = node->sequence_length;
        uint32_t freq = seq_freq_get(seq, len);
        double savings = calc_savings(node, freq);

        if (savings > max_savings) {
            max_savings = savings;
            best_node = node;
        }
    }

    return best_node;
}


/**
 * Prints either the current path or the best path.
 * @param isCurrent If true, prints current path; otherwise prints best path
 * @param shouldPrintData If true, prints sequence details as well
 * @param block Pointer to input block (for sequence data)
 */
static void print_path(uint8_t isCurrent, uint8_t shouldPrintData, const uint8_t *block) {
    const int idx = isCurrent ? PATH_CURRENT : PATH_BEST;
    const int32_t size = path_state.path_size[idx];
    const double total_cost = path_state.path_total_cost[idx];
    const uint32_t *stack = path_state.path_stack[idx];
    const uint32_t *freqs = path_state.path_freqs[idx];
    const double *per_node_costs = path_state.path_per_node_costs[idx];

    printf("\n=== %s PATH ===\n", isCurrent ? "CURRENT" : "BEST");
    printf("Path size = %d, Total cost = %.2lf\n", size + 1, total_cost);
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
        const double cost = per_node_costs[i];

        printf(" -> ");
        if (node->is_RLE) {
            printf("RLE=YES ");
        }

        printf("| id=%u len=%u freq=%u cost=%.2f | ",
               node->node_id, len, freq, cost);

        print_node_sequence(node, block);

        if (i % 20 == 0) fflush(stdout);
    }

    printf("\n\n");
}


/**
 * Performs a backward traversal of the sequence graph to identify the optimal
 * (lowest-cost) path from the last level (source) to the first level (sink),
 * using a multi-stage refinement process based on sequence frequencies.
 *
 * Assumptions:
 * - Each node in the graph represents a sequence which can be shared by multiple nodes of the graph.
 * - Sequence frequency and usefulness can be queried/updated via functions in `seq_freq_map.h`.
 * - Initially, all sequences are marked as `useful = 0` (i.e., not on the optimal path).
 *
 * The algorithm runs in multiple stages to progressively refine the set of useful sequences.
 *
 * --------------------
 * Stage 1 (Backward Path Tracing):
 * --------------------
 * 1. Start at the last level (e.g., level N of the graph).
 * 2. For each node at this level, identify the node with the minimum cost using `calc_cost(...)`.
 * 3. Mark the sequence associated with that node as useful in the sequence frequency map.
 * 4. Recursively trace backward from the selected node to its parent nodes (i.e., nodes at the previous level).
 * 5. At each level, again select the node with the minimum cost (using `calc_cost(...)`) and mark it as useful.
 * 6. Continue this process until the root level (first level) is reached.
 * 7. If this traversal produces a **unique path** from the source node (last level) to the sink node (first level),
 *    the process terminates.
 *
 * --------------------
 * Stage 2 (Frequency Recalculation):
 * --------------------
 * 1. Recompute the frequency of each sequence, **considering only the sequences marked as useful** in Stage 1.
 *    All other sequences are ignored during frequency counting.
 * 2. Reset previous usefulness markings.
 * 3. Re-execute Stage 1 with updated frequency data.
 *
 * Repeat Stage 1 and Stage 2 iteratively until a **unique optimal path** is identified from source to sink.
 *
 * Notes:
 * - The algorithm balances frequency-based cost metrics with path tracing to incrementally converge
 *   on the minimal cost sequence path.
 * - Uniqueness of the path ensures deterministic sequence selection.
 */
void find_shortest_path_to_sink(const uint8_t *block) {

    // Initialize path state
    memset(&path_state, 0, sizeof(Path));
    uint16_t level = get_last_level_index(); //last leve.
    GraphNode *node = find_min_cost_node_at_level(level, block);
    path_state.path_stack[PATH_BEST][path_state.path_size[PATH_BEST]++] = node->node_id;
    while (node->node_id != 0) { //continue till we reach the root node.
        level = get_parent_level(node);
        node = find_min_cost_node_at_level(level, block);
        path_state.path_stack[PATH_BEST][path_state.path_size[PATH_BEST]++] = node->node_id;
    }
    print_path(0, 1, block);
}



void old_fun(const uint8_t *block) {
    // Initialize path state
    memset(&path_state, 0, sizeof(Path));

    // Get the last level index
    uint16_t last_level = get_last_level_index();
    if (last_level == 0) {
        // Empty graph case
        return;
    }

    // Stage 1: Backward Path Tracing
    bool path_found = false;
    int iterations = 0;
    const int MAX_ITERATIONS = 10; // Prevent infinite loops

    while (!path_found && iterations++ < MAX_ITERATIONS) {
        // Reset path state for new iteration
        path_state.path_size[PATH_CURRENT] = 0;
        path_state.path_total_cost[PATH_CURRENT] = 0;
        path_state.freq_eq_1_count = 0;

        // Start from each node in the last level
        uint32_t start_node_id = get_level_start_id(last_level);
        uint32_t end_node_id = get_level_end_id(last_level);

        for (uint32_t node_id = start_node_id; node_id < end_node_id; node_id++) {
            GraphNode *node = get_graph_node(node_id);

            // Reset current path for this starting node
            path_state.path_size[PATH_CURRENT] = 0;
            path_state.path_total_cost[PATH_CURRENT] = 0;
            path_state.freq_eq_1_count = 0;

            // Trace path backward from this node
            GraphNode *current_node = node;
            bool reached_root = false;

            while (!reached_root) {
                // Get sequence frequency and usefulness
                const uint8_t *seq = block + current_node->offset;
                uint8_t len = current_node->sequence_length;
                uint32_t freq = seq_freq_get(seq, len);

                // Calculate cost for this node
                double cost = calc_cost(current_node, freq);

                // Add to current path
                if (path_state.path_size[PATH_CURRENT] < MAX_LEVELS) {
                    path_state.path_stack[PATH_CURRENT][path_state.path_size[PATH_CURRENT]] = current_node->node_id;
                    path_state.cost_stack[PATH_CURRENT][path_state.path_size[PATH_CURRENT]] = cost;
                    path_state.path_freqs[PATH_CURRENT][path_state.path_size[PATH_CURRENT]] = freq;
                    path_state.path_per_node_costs[PATH_CURRENT][path_state.path_size[PATH_CURRENT]] = cost;
                    path_state.path_total_cost[PATH_CURRENT] += cost;
                    path_state.path_size[PATH_CURRENT]++;

                    if (freq == 1) {
                        path_state.freq_eq_1_count++;
                    }
                }

                // Mark as useful
                seq_usefull_set(seq, len, 1);

                // Check if we reached root
                if (current_node->node_id == 0) {
                    reached_root = true;
                    break;
                }

                // Find best parent (minimum cost)
                uint8_t parent_count = get_parent_nodes_count(current_node);
                if (parent_count == 0) {
                    break; // Shouldn't happen for valid graph
                }

                GraphNode *parents = get_parent_nodes(current_node);
                double min_cost = DBL_MAX;
                GraphNode *best_parent = NULL;

                for (uint8_t i = 0; i < parent_count; i++) {
                    GraphNode *parent = &parents[i];
                    const uint8_t *parent_seq = block + parent->offset;
                    uint8_t parent_len = parent->sequence_length;
                    uint32_t parent_freq = seq_freq_get(parent_seq, parent_len);
                    double parent_cost = calc_cost(parent, parent_freq);

                    if (parent_cost < min_cost) {
                        min_cost = parent_cost;
                        best_parent = parent;
                    }
                }

                if (!best_parent) {
                    break; // No valid parent found
                }

                current_node = best_parent;
            }

            // Check if this path is better than the current best
            if (path_state.path_size[PATH_CURRENT] > 0 &&
                (path_state.path_size[PATH_BEST] == 0 ||
                 path_state.path_total_cost[PATH_CURRENT] < path_state.path_total_cost[PATH_BEST])) {
                // Copy current path to best path
                memcpy(path_state.path_stack[PATH_BEST], path_state.path_stack[PATH_CURRENT],
                       sizeof(uint32_t) * path_state.path_size[PATH_CURRENT]);
                memcpy(path_state.cost_stack[PATH_BEST], path_state.cost_stack[PATH_CURRENT],
                       sizeof(int32_t) * path_state.path_size[PATH_CURRENT]);
                memcpy(path_state.path_freqs[PATH_BEST], path_state.path_freqs[PATH_CURRENT],
                       sizeof(uint32_t) * path_state.path_size[PATH_CURRENT]);
                memcpy(path_state.path_per_node_costs[PATH_BEST], path_state.path_per_node_costs[PATH_CURRENT],
                       sizeof(double) * path_state.path_size[PATH_BEST]);
                path_state.path_size[PATH_BEST] = path_state.path_size[PATH_CURRENT];
                path_state.path_total_cost[PATH_BEST] = path_state.path_total_cost[PATH_CURRENT];
            }
        }

        // Check if we found a unique path
        if (path_state.path_size[PATH_BEST] > 0 && path_state.freq_eq_1_count == 0) {
            path_found = true;
        } else {
            // Stage 2: Frequency Recalculation
            // Reset all usefulness flags
            for (uint32_t i = 0; i < graph.size; i++) {
                GraphNode *node = get_graph_node(i);
                const uint8_t *seq = block + node->offset;
                seq_usefull_set(seq, node->sequence_length, 0);
            }

            // Recompute frequencies considering only useful sequences
            init_seq_freq_map(); // reset by removing old frequencies.

            for (uint32_t i = 0; i < total_input_size; i++) {
                for (uint8_t len = 1; len <= SEQ_LENGTH_LIMIT && i + len <= total_input_size; len++) {
                    const uint8_t *seq = block + i;

                    // Only increment if marked useful
                    if (seq_usefull_get(seq, len)) {
                        seq_freq_increment(seq, len);
                    }
                }
            }
        }
    }

    // If we found a path, print it.
    if (path_found) {
        for (int i = 0; i < path_state.path_size[PATH_BEST]; i++) {
            uint32_t node_id = path_state.path_stack[PATH_BEST][i];
            GraphNode *node = get_graph_node(node_id);
            print_graph_node(node);
        }
    }
}

void free_path_state() {
    // Only if path_state has dynamic allocations
    memset(&path_state, 0, sizeof(Path));
}