// shortest_path.c

#include "shortest_path.h"
#include "../map/seq_freq_map.h"


int prune_count =0;

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


typedef struct {
    uint32_t node_id;
    uint32_t node_id_popped;
    uint32_t hash_index_cache;
} StackItem;

typedef struct {
    uint32_t current_path_stack[MAX_LEVELS];
    int32_t cost_stack[MAX_LEVELS]; // cost added by each node
    int32_t current_path_size;
    double current_path_cost;
    int32_t single_freq_sequence_count;
    uint32_t best_path_stack[MAX_LEVELS];
    double best_path_cost;
    int32_t best_path_size;
} Path;

Path path_state;
SeqFreqMap map;

/**
 * Initializes the path state for a new search.
 */
static inline void path_init() {
    path_state.current_path_size = -1;    
    path_state.current_path_cost = 0;
    path_state.single_freq_sequence_count = 0;
    path_state.best_path_size = -1;
    path_state.best_path_cost = INT32_MAX;
}

/**
 * Updates the best path if the current path is better.
 */
static inline uint8_t update_best_path() {    
    if (path_state.current_path_cost < path_state.best_path_cost || 
    (path_state.current_path_cost == path_state.best_path_cost && 
        path_state.current_path_size < path_state.best_path_size)) {
        path_state.best_path_cost = path_state.current_path_cost;
        memcpy(path_state.best_path_stack, path_state.current_path_stack,
               (path_state.current_path_size + 1) * sizeof(uint32_t));
        path_state.best_path_size = path_state.current_path_size;
        return 1;
        
    }
    return 0;
}

/**
 * Prints either the current path or the best path.
 * @param isCurrent If true, prints current path; otherwise prints best path
 */
static void print_path(uint8_t isCurrent, uint8_t shouldPrintData, const uint8_t* block) {
    const int32_t size = isCurrent ? path_state.current_path_size : path_state.best_path_size;
    const double cost = isCurrent ? path_state.current_path_cost : path_state.best_path_cost;
    const uint32_t *stack = isCurrent ? path_state.current_path_stack : path_state.best_path_stack;
    
    printf("\nShortest path size=%d, cost=%lf \n", size, cost);    
    for (int32_t i = size; i >= 0; i--) {
        uint32_t node_id = stack[i];
        GraphNode *node = get_graph_node(node_id);
        printf("(%u,%u)", stack[i], node->node_level);
        if (i-1 >= 0) {
            printf("->");
        }
    }
    printf("\n\n\n");
    if (shouldPrintData) {
        for (int32_t i = size; i >= 0; i--) {
            uint32_t node_id = stack[i];
            GraphNode *node = get_graph_node(node_id);
            if (!node) continue;            
            //printf(" -> %u| ", node->hash_index_cache);
            printf(" -> ");
            print_node_sequence(node, block);
            printf("\n");
            if (i%20 == 0) {
                fflush(stdout);
            }
        }
        printf("\n\n");
    }
}

/**
 * Handles backtracking by removing the node from current path,
 * decreasing its frequency if needed, and updating costs.
 */
static inline void backtrack_node(uint32_t node_id, const uint8_t* block) {
    GraphNode *node = get_graph_node(node_id);
#ifdef DEBUG
    printf("backtrack node %u\n", node->node_id);
#endif

    path_state.current_path_cost -= path_state.cost_stack[path_state.current_path_size];
    if (node->sequence_length > 1) {
            seq_freq_decrement(&map, &block[node->offset], node->sequence_length, node->node_id);        
    }

    path_state.current_path_size--;
}


/**
 * Processes a node by adding it to the current path, updating frequencies,
 * and calculating its cost contribution.
 */
static inline void process_node(const uint8_t* block, GraphNode* node) {
#ifdef DEBUG
    printf("Push node %u\n", node->node_id);
#endif

    path_state.current_path_stack[++path_state.current_path_size] = node->node_id;

    uint32_t freq = 1;
    if (node->sequence_length > 1) {
        freq = seq_freq_increment(&map, &block[node->offset], node->sequence_length, node->node_id);
        //printf("\n node_id=%u, freq=%d \n", node->node_id, freq);
    }
    
    double added_cost = calc_cost(node, freq);
    if (node->node_id == 0) added_cost = 0;
    path_state.cost_stack[path_state.current_path_size] = added_cost;
    path_state.current_path_cost += added_cost;
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
    if (path_state.current_path_cost + node->min_depth > path_state.best_path_cost ||
        (path_state.current_path_cost + node->min_depth == path_state.best_path_cost &&
         path_state.current_path_size + node->min_depth > path_state.best_path_size)) {
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
    path_state.current_path_size = -1; // remove current path but keep the best path.
    path_state.current_path_cost = 0;
    path_state.single_freq_sequence_count = 0;
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
             path_state.single_freq_sequence_count = 0; //reset.
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
                printf("Saved the path %d with cost: %lf\n", best_count, path_state.current_path_cost);
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

}