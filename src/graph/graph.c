#include "graph.h"

#define PER_LEVEL_GRAPH_NODES(SEQ_LIMIT, LEVEL) \
    (((LEVEL) <= (SEQ_LIMIT)) ? ((LEVEL) * ((LEVEL) + 1)) / 2 \
                              : ((SEQ_LIMIT) * ((SEQ_LIMIT) + 1)) / 2 + ((LEVEL) - (SEQ_LIMIT)) * (SEQ_LIMIT))


Graph graph; // Actual single definition

/**
 * Tracks if a sequence exist at a level i its ancestors.
 */ 
SequenceRepository exist_repo[MAX_LEVELS];

void init_graph(void) {
    graph.size = 0;
    graph.total_levels = 0;    
    memset(graph.first_node_of_level, 0, sizeof(graph.first_node_of_level));
}

// Verification function of the graph.
void verify_graph_integrity() {
    for (uint32_t i = 1; i < graph.size; i++) {
        GraphNode* node = &graph.nodes[i];
        if (node->isUseless) {
            fprintf(stderr, "\nStill found a useless node. There should be None. node_id=%u, node_level=%u \n\n", node->node_id, node->node_level);
            abort();
        }
        uint16_t parent_level = node->node_level - node->sequence_length;
        if (parent_level >= graph.total_levels) {
            fprintf(stderr, "Node %u has invalid parent level %u\n",
                   node->node_id, parent_level);
            abort();
        }
    }
}

/**
 * compact_graph:
 *
 * This function performs two critical tasks in a single O(n) pass over the graph:
 *
 * (1) Compaction:
 *     - Removes all nodes marked as `isUseless`.
 *     - Maintains topological ordering based on level indices.
 *     - Updates `first_node_of_level` to reflect the new compacted layout.
 *
 * (2) Minimum Depth Calculation:
 *     - Computes the minimum depth (`min_depth`) of each node from the root node (node_id = 0).
 *     - Root node is at depth 0.
 *     - All parent nodes of any node lie in the same level, determined by:
 *           parent_level = node_level - sequence_length
 *     - Because of level-based topological ordering, we can track each level’s minimum depth while compacting.
 *     - This allows us to assign a node’s depth in constant time:
 *           min_depth(node) = 1 + level_min_depth[parent_level]
 *
 * Prerequisites:
 * - `graph` must be fully constructed with all nodes.
 * - All `isUseless` flags must be correctly set.
 *
 * Side effects:
 * - Modifies `graph.nodes`, `graph.size`, `graph.first_node_of_level`, and sets `min_depth` for each node.
 */
void compact_graph(const uint8_t* block) {
    assert(graph.size == 0 || (graph.nodes[0].node_id == 0 && !graph.nodes[0].isUseless));

    uint32_t write_idx = 0;
    //we start from level 0 or sink and go towards source nodes.
    uint32_t current_level = 0;
    uint32_t level_start = 0;
#ifdef DEBUG
    printf("\n\nTotal nodes before compaction =%u\n", graph.size);
#endif 
    // Initialize root node's min depth
    graph.nodes[0].min_depth = 0;
    graph.level_min_depth[0] = 0;

    for (uint32_t read_idx = 0; read_idx < graph.size; read_idx++) {
        // Detect level transition
        if (current_level + 1 < graph.total_levels &&
            read_idx >= graph.first_node_of_level[current_level + 1]) {

            // Record compacted start of the current level
            graph.first_node_of_level[current_level] = level_start;
#ifdef DEBUG
        printf("Level %u sequences:\n", current_level);
        seq_repo_print_all(&exist_repo[current_level]);

#endif                        
            current_level++;
            level_start = write_idx;

            //First we initialize the new map.
            seq_repo_init(&exist_repo[current_level], PER_LEVEL_GRAPH_NODES(SEQ_LENGTH_LIMIT, current_level));

        }

        GraphNode* node = &graph.nodes[read_idx];
        if (node->isUseless) {
            continue;
        }

        // Compact node to new position
        if (write_idx != read_idx) {
            graph.nodes[write_idx] = *node;            
        }

        GraphNode* new_node = &graph.nodes[write_idx];
        new_node->node_id = write_idx;
        // Here we create map that contain sequence of new_node union with
        // sequences of its parent_level.        
        uint16_t parent_level = new_node->node_level - new_node->sequence_length;     
        
        // Merge parent level’s exist_repo into current level
        SequenceRepository *src = &exist_repo[parent_level];
        SequenceRepository *dst = &exist_repo[current_level];
        for (uint32_t j = 0; j < src->capacity; j++) {
            if (src->is_used[j]) {
                seq_repo_increase_frequency(dst, src->entries[j].data,  src->entries[j].length);
            }
        }        
        
        if (new_node->sequence_length > 1) {
            // Add current node's sequence to the current level's exist_repo
            seq_repo_increase_frequency(&exist_repo[current_level], &block[new_node->offset],
                        new_node->sequence_length);
        }        

        // Compute min_depth if not the root node
        if (write_idx != 0) {
            uint16_t parent_level = new_node->node_level - new_node->sequence_length;

            // Defensive check: parent_level must be in range
            if (parent_level >= graph.total_levels) {
                fprintf(stderr, "Invalid parent level %u for node %u\n", parent_level, new_node->node_id);
                exit(1);
            }

            uint16_t parent_min_depth = graph.level_min_depth[parent_level];
            new_node->min_depth = parent_min_depth + 1;
        }

        // Track level's minimum depth
        if (write_idx == level_start) {
            graph.level_min_depth[current_level] = new_node->min_depth;
        } else if (new_node->min_depth < graph.level_min_depth[current_level]) {
            graph.level_min_depth[current_level] = new_node->min_depth;
        }

        write_idx++;
    }

    // Final level boundary
    graph.first_node_of_level[current_level] = level_start;

    // Update remaining levels if any
    while (++current_level < graph.total_levels) {
        graph.first_node_of_level[current_level] = write_idx;
    }

    // Resize graph
    graph.size = write_idx;

    #ifdef DEBUG
    printf("Total nodes after compaction =%u\n", graph.size);
    verify_graph_integrity();
    #endif
}

static inline void print_node_link(GraphNode* node, GraphNode* parent) {
    printf("\t %u --> %u\n", node->node_id, parent->node_id);
}

void print_node_sequence(GraphNode *node, const uint8_t* block) {
    for (int i=0; i<node->sequence_length; i++) {
        printf("%0x", block[node->offset+i]);
        if (i+1 < node->sequence_length) {
            printf(",");
        }
        
    }
    //printf("\n");
}

void print_graph_node(GraphNode *node) {
    if (!node) return;

    if (node->node_id == 0) {
        printf("\nROOT NODE ");
    } else {
        printf("\n");
    }
    uint16_t parent_nodes_count = get_parent_nodes_count(node);
    printf("node_id = %u, start_of_sequence = %u, sequence_length = %u",
           node->node_id, node->offset, node->sequence_length);
    printf(", parent_count = %u\n", parent_nodes_count);
    GraphNode* parent_nodes = get_parent_nodes(node);
    if (!parent_nodes) return;
    for (int i = 0; i < parent_nodes_count; i++) {
        print_node_link(node, &parent_nodes[i]);
        break;// just print one link per node as other belings to the same level.
    }

}