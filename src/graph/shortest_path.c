// shortest_path.c

#include "shortest_path.h"
#include "../map/seq_freq_map.h"


int prune_count =0;


typedef struct {
    uint32_t node_id;
    uint32_t node_id_popped;
    uint32_t hash_index_cache;
} StackItem;


Path path_state;
static SeqFreqMap map;


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
 * Initializes the path state for a new search.
 */
static inline void path_init() {
    path_state.path_size[PATH_CURRENT] = -1;
    path_state.path_size[PATH_BEST] = -1;
    path_state.path_total_cost[PATH_CURRENT] = 0;
    path_state.path_total_cost[PATH_BEST] = INT32_MAX; 
    path_state.path_size[PATH_CURRENT] = -1;
    path_state.path_size[PATH_BEST] = -1;
    path_state.freq_eq_1_count = 0;
}

/**
 * Updates the best path if the current path is better.
 */
static inline uint8_t update_best_path() {
    //if (path_state.freq_eq_1_count > 0) return 0;  // Invalid: contains freq==1 sequences

    if (path_state.path_total_cost[PATH_CURRENT] < path_state.path_total_cost[PATH_BEST] || 
        (path_state.path_total_cost[PATH_CURRENT] == path_state.path_total_cost[PATH_BEST] &&
         path_state.path_size[PATH_CURRENT] < path_state.path_size[PATH_BEST])) {

        int32_t size = path_state.path_size[PATH_CURRENT] + 1;

        path_state.path_total_cost[PATH_BEST] = path_state.path_total_cost[PATH_CURRENT];
        path_state.path_size[PATH_BEST] = path_state.path_size[PATH_CURRENT];

        memcpy(path_state.path_stack[PATH_BEST], path_state.path_stack[PATH_CURRENT], size * sizeof(uint32_t));
        memcpy(path_state.cost_stack[PATH_BEST], path_state.cost_stack[PATH_CURRENT], size * sizeof(int32_t));
        memcpy(path_state.path_freqs[PATH_BEST], path_state.path_freqs[PATH_CURRENT], size * sizeof(uint32_t));
        memcpy(path_state.path_per_node_costs[PATH_BEST], path_state.path_per_node_costs[PATH_CURRENT], size * sizeof(double));

        return 1;
    }

    return 0;
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
     * Handles backtracking by removing the node from current path,
     * decreasing its frequency if needed, and updating costs.
     */
    static inline void backtrack_node(uint32_t node_id, const uint8_t *block) {
        GraphNode *node = get_graph_node(node_id);
#ifdef DEBUG
    printf("backtrack node %u\n", node->node_id);
#endif

    path_state.path_total_cost[PATH_CURRENT] -= path_state.cost_stack[PATH_CURRENT][path_state.path_size[PATH_CURRENT]];
    if (node->sequence_length > 1 && !node->is_RLE) {
            uint32_t new_freq = seq_freq_decrement(&map, &block[node->offset], node->sequence_length, node->node_id);
            if (new_freq == 1) {
                path_state.freq_eq_1_count++;
            } else if (new_freq == 0) {
                path_state.freq_eq_1_count--;  // sequence completely removed
            }
#ifdef DEBUG
            if (node->sequence_length > 1) {
                uint32_t freq = seq_freq_get(&map, &block[node->offset], node->sequence_length, node->node_id);
                printf("DECR: node_id=%u, new_freq=%u, seq=", node->node_id, freq);
                print_node_sequence(node, block);

            }
#endif
    }

    path_state.path_size[PATH_CURRENT]--;
}


/**
 * Processes a node by adding it to the current path, updating frequencies,
 * and calculating its cost contribution.
 */
static inline void process_node(const uint8_t* block, GraphNode* node) {
#ifdef DEBUG
    printf("Push node %u\n", node->node_id);
#endif

    path_state.path_stack[PATH_CURRENT][++path_state.path_size[PATH_CURRENT]] = node->node_id;

    uint32_t freq = 1;
    if (node->sequence_length > 1 && !node->is_RLE) {
        freq = seq_freq_increment(&map, &block[node->offset], node->sequence_length, node->node_id);
        //printf("\n node_id=%u, freq=%d \n", node->node_id, freq);
        if (freq == 1) {
                path_state.freq_eq_1_count++;
        } else if (freq == 2) {
                path_state.freq_eq_1_count--;  // sequence is now repeated
        }        
#ifdef DEBUG
        if (node->sequence_length > 1) {
            printf("INCR: node_id=%u, freq=%u, seq=", node->node_id, freq);
            print_node_sequence(node, block);
        }
#endif
    }
    
    double added_cost = calc_cost(node, freq);
    if (node->node_id == 0) added_cost = 0;
    path_state.cost_stack[PATH_CURRENT][path_state.path_size[PATH_CURRENT]] = added_cost;
    path_state.path_total_cost[PATH_CURRENT] += added_cost;
    path_state.path_freqs[PATH_CURRENT][path_state.path_size[PATH_CURRENT]] = freq;
    path_state.path_per_node_costs[PATH_CURRENT][path_state.path_size[PATH_CURRENT]] = added_cost;

}


/**
 * Initializes the DFS stack with all valid leaf nodes.
 */
static inline void initialize_leaf_nodes(StackItem* stack, int* top, uint16_t last_level, uint32_t first_node_start) {
    uint32_t start = get_level_start_id(last_level);
    uint32_t end = get_level_end_id(last_level);
    if (start+first_node_start >= end) return;
    for (uint32_t i = start+first_node_start; i < end; i++) {
        GraphNode *node = get_graph_node(i);
        if (node) {            
            stack[++(*top)] = (StackItem){.node_id = i, .node_id_popped = 0};
        } 
    }
}

static inline uint8_t should_prune(GraphNode *node) {
    if (path_state.path_total_cost[PATH_CURRENT] + node->min_depth > path_state.path_total_cost[PATH_BEST] ||
        (path_state.path_total_cost[PATH_CURRENT] + node->min_depth == path_state.path_total_cost[PATH_BEST] &&
         path_state.path_size[PATH_CURRENT] + node->min_depth > path_state.path_size[PATH_BEST])) {
            prune_count++;
            return 1;
    }
    return 0;
}

/**
 * Adds all valid parent nodes to the DFS stack for exploration.
 */
static inline void add_parent_nodes_to_stack(StackItem *stack, int *top,
                                             GraphNode *node,
                                             const uint8_t *block) {
    uint8_t parent_count = get_parent_nodes_count(node);
    GraphNode *parents = get_parent_nodes(node);

    for (uint8_t i = 0; i < parent_count; i++) {
        GraphNode *parent = &parents[i];
        if (should_prune(parent) /*&& (node->node_level < get_last_level_index()-1 && parent->min_depth == node->min_depth)*/) {
#ifdef DEBUG
            printf("Prune by cost: parent node_id=%u\n", parent->node_id);
#endif
            continue;
        }

        // Passed all pruning checks, push to stack
        stack[++(*top)] = (StackItem){
            .node_id = parent->node_id,
            .node_id_popped = 0
        };
    }
}


static inline uint8_t start_fresh_from_another_leaf(int *top, StackItem *main_stack, uint16_t last_level, 
    uint32_t node_of_last_level_served, uint32_t* push_count ) {
    *top = -1; // stack is empty again. Start fresh.
    initialize_leaf_nodes(main_stack, top, last_level, node_of_last_level_served); // start again from the next leaf.
    if (*top == -1)
        return 0; // all last level nodes has been served.
    *push_count = 0;
    path_state.path_size[PATH_CURRENT] = -1; // remove current path but keep the best path.
    path_state.path_total_cost[PATH_CURRENT] = 0;

    return 1;
}

/**
 * Performs a DFS-based traversal (using a manual stack to avoid recursion)
 * to find the shortest-cost path from any leaf node to the root node (node_id
 * == 0) in a DAG (Directed Acyclic Graph) representing a compression graph.
 *
 * The path cost is computed based on the frequency and length of sequences at
 * each node using COST Macro.
 */
void find_shortest_path_to_sink(const uint8_t *block) {
    long stack_size = MIN(total_input_size+1, BLOCK_SIZE);//note: there is one extra level with no data. Count it!
    long max_push = 10*get_graph_size();
    StackItem main_stack[stack_size];
    int top = -1;
    uint16_t last_level = get_last_level_index();
    uint32_t back_track_count = 0;
    uint32_t best_count = 0;
    uint32_t push_count = 0;
    uint32_t node_of_last_level_served = 0;
    path_init();      // Reset path state
    init_seq_freq_map(&map, stack_size);
    initialize_leaf_nodes(main_stack, &top, last_level, 0);

    while (top >= 0) {
        if (push_count > max_push && best_count >= 1 
            && !start_fresh_from_another_leaf(&top, main_stack, last_level, node_of_last_level_served, &push_count)) {
            break;            
        }
        StackItem current = main_stack[top--];

        if (current.node_id == UINT32_MAX) {
            // Backtrack marker encountered
            backtrack_node(current.node_id_popped, block);
            back_track_count++;
            continue;
        }
        GraphNode *node = get_graph_node(current.node_id);
        if (node->node_id >= get_level_start_id(last_level)) { //encountered a leaf node.
             node_of_last_level_served++;
        }

        // The following pruning is very useful for speed up.
        // In it, we do not explore paths which are worse.
        // Early pruning before frequency cost and stack updates
        if (should_prune(node)) {
#ifdef DEBUG
                printf("Prune longer path node_id=%u\n", node->node_id);
#endif               
            
            continue;
        }

        process_node(block, node);
        push_count++;

        // Push backtrack marker
        main_stack[++top] = (StackItem){.node_id = UINT32_MAX, .node_id_popped = current.node_id};

        // If reached root node (ID 0), check and update best path
        if (current.node_id == 0) {
            
#ifdef DEBUG
            print_path(1, 1, block);
#endif
            if(update_best_path()) {
                best_count++;
                printf("Saved the path %d with cost: %lf\n", best_count, path_state.path_total_cost[PATH_CURRENT]);
                printf("\nbest_count=%u, prune_count=%u, back_track_count=%u, push_count=%u\n", best_count, prune_count, back_track_count, push_count);
                push_count = 0;
                back_track_count = 0;
                prune_count = 0;
            }
            continue; // Root has no parents
        }

        // Explore parents 
        add_parent_nodes_to_stack(main_stack, &top, node, block);
    }
    printf("\nbest_count=%u, prune_count=%u, back_track_count=%u, push_count=%u\n", best_count, prune_count, back_track_count, push_count);
    // Final output
//#ifdef DEBUG    
    print_path(0, 1, block);
//#endif
    //free_path_state();
}


void free_path_state() {
    // Only if path_state has dynamic allocations
    free_seq_freq_map(&map);  // Example if map needs freeing
    memset(&path_state, 0, sizeof(Path));
}