#include "graph.h"

#define PER_LEVEL_GRAPH_NODES(SEQ_LIMIT, LEVEL) \
    ((LEVEL) == 0 ? 1 : \
    ((LEVEL) <= (SEQ_LIMIT)) ? ((LEVEL) * ((LEVEL) + 1)) / 2 \
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
 * For each graph level, this function builds a repository (`exist_repo[level]`)
 * containing all sequences (length > 1) that appear in that level or above.
 * 
 * Traverses all nodes in level order and accumulates their sequences,
 * along with sequences inherited from parent levels.
 */
static void record_level_wize_nodes(const uint8_t* block) {
    assert(graph.size == 0 || (graph.nodes[0].node_id == 0 && !graph.nodes[0].isUseless));

#ifdef DEBUG
    printf("\n\nStart level_wize node recording graph size= %u\n", graph.size);
#endif

    // Initialize root node's depth and its empty repository
    graph.nodes[0].min_depth = 0;
    graph.level_min_depth[0] = 0;

    uint32_t last_processed_level = UINT32_MAX;

    for (uint32_t i = 0; i < graph.size; i++) {
        GraphNode* node = &graph.nodes[i];

        uint32_t level = node->node_level;

        // Initialize new level's repository when first node of a new level is encountered
        if (level != last_processed_level) {
            seq_repo_init(&exist_repo[level], PER_LEVEL_GRAPH_NODES(SEQ_LENGTH_LIMIT, level));
            last_processed_level = level;

#ifdef DEBUG
            printf("Initialized sequence repository for level %u, with capacity=%u\n", level, exist_repo[level].capacity);
#endif
        }

        // Merge parent level’s sequences into current level
        uint32_t parent_level = level - node->sequence_length;
        SequenceRepository* parent_repo = &exist_repo[parent_level];
        SequenceRepository* current_repo = &exist_repo[level];

        for (uint32_t j = 0; j < parent_repo->capacity; j++) {
            if (parent_repo->is_used[j]) {
                seq_repo_increase_frequency(current_repo,
                                            parent_repo->entries[j].data,
                                            parent_repo->entries[j].length);
            }
        }

        // Add current node's sequence if length > 1
        if (node->sequence_length > 1) {
            seq_repo_increase_frequency(current_repo,
                                        &block[node->offset],
                                        node->sequence_length);
        }
    }

#ifdef DEBUG
    for (uint32_t level = 0; level <= last_processed_level; level++) {
        printf("Level %u sequences:\n", level);
        seq_repo_print_all(&exist_repo[level]);
    }
    printf("Finished creating per-level sequence repositories.\n");
#endif
}


static inline uint8_t check_useless(uint8_t size_pre, uint32_t* candidates_pre,
    const uint8_t* seq, uint8_t len, const uint8_t* block) {
    for (int i = 0; i < size_pre; i++) {
        uint32_t c_node_id = candidates_pre[i];
        GraphNode *c_node = get_graph_node(c_node_id);
        const uint8_t *c_seq = &block[c_node->offset];
        const uint8_t c_len = c_node->sequence_length;
        if (c_len != len)
            continue;
        if (sequences_equal(seq, c_seq, len)) {
            return c_node_id;
        }
    } 
    return 0;
}

/**
 * Marks graph nodes as useless based on sequence frequency analysis.
 *
 * Step 1:
 * - A node is marked useless if its sequence appears only once in the entire graph.
 * - Since `exist_repo[last_level]` accumulates all sequences from all levels, 
 *   a frequency of 1 means the sequence is unique and not reused anywhere else.
 *
 * Step 2:
 * - A node may appear to have duplicates due to overlapping substrings (e.g., AAAA → AAA, AAA).
 * - We count how many times the node's sequence is absent from the parent's level.
 * - If this absence count equals the sequence frequency at the last level,
 *   it means the sequence never appeared without overlapping itself.
 *   Such nodes are also marked useless.
 */
static void mark_nodes_useless(const uint8_t* block) {
    const uint16_t last_level = get_last_level_index();
    uint32_t freq_2_useless_candidate1[SEQ_LENGTH_LIMIT]; //of level-1
    uint8_t size1 = 0;
    uint32_t freq_2_useless_candidate2[SEQ_LENGTH_LIMIT]; //of level-1
    uint8_t size2 = 0;
    uint8_t current_array = 1;
    uint16_t level = 0;
    #ifdef DEBUG
    printf("\n\nMarking useless nodes of two different kind with graph size= %u\n", graph.size);
#endif

    for (uint32_t i = 0; i < graph.size; i++) {
        GraphNode* node = &graph.nodes[i];
#ifdef DEBUG
        printf("Processing node=%u\n", node->node_id);
#endif
        if (node->sequence_length <= 1 || node->isUseless) {
            continue; // Ignore very short or already-marked nodes
        }
        if (node->node_level != level) { //change of level            
            current_array = current_array == 1? 2 : 1;
            if (current_array == 1) {
                size1 = 0;
            } else {
                size2 = 0;
            }
        }
        level = node->node_level;
        const uint16_t parent_level = level - node->sequence_length;
        const uint8_t* seq = &block[node->offset];
        const uint8_t len = node->sequence_length;

        // Step 1: Remove globally unique sequences
        uint32_t freq_in_graph = seq_repo_get_frequency(&exist_repo[last_level], seq, len);
        if (freq_in_graph == 1) {
            node->isUseless = 1;
#ifdef DEBUG
            printf("Marked node_id=%u as USELESS (unique in graph)\n", node->node_id);
#endif
            continue; // Skip Step 2 if already useless
        }

        // Step 2: Check if all appearances are due to overlaps

        //a) Is the sequence of node has freq 2 at the last level. If not then continue.
        if (seq_repo_get_frequency(&exist_repo[last_level], seq, len) > 2) {
            continue;
        }

        //b) If parent level has frequency greater than 0 then continue
        if (seq_repo_get_frequency(&exist_repo[parent_level], seq, len) > 0) {
            continue;
        }

        if (current_array == 1) {
            uint32_t c_node_id = check_useless(size2, freq_2_useless_candidate2, seq, len, block);
            if (c_node_id) {
#ifdef DEBUG
            printf("Marked node_id=%u and %u as USELESS (only overlaps)\n", node->node_id, c_node_id);
#endif
            get_graph_node(c_node_id)->isUseless = 1;
            node->isUseless = 1;
            } else {
                freq_2_useless_candidate1[size1++] = node->node_id;
            }
        } else {
            uint32_t c_node_id = check_useless(size1, freq_2_useless_candidate1,
                                               seq, len, block);
            if (c_node_id) {
#ifdef DEBUG
                printf("Marked node_id=%u and %u as USELESS (only overlaps)\n",
                       node->node_id, c_node_id);
#endif

                get_graph_node(c_node_id)->isUseless = 1;
                node->isUseless = 1;

            } else {
                freq_2_useless_candidate2[size2++] = node->node_id;
            }
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
    
    record_level_wize_nodes(block);
    mark_nodes_useless(block);

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
        printf("%c", block[node->offset+i]);
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