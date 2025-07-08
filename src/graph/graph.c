#include "graph.h"
#include "../map/seq_freq_map.h"
#include "timer.h"

#define MAX_CONSEC_LEVELS 10
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

    uint32_t last_processed_level = UINT32_MAX;

    for (uint32_t i = 0; i < graph.size; i++) {
        GraphNode* node = &graph.nodes[i];

        uint32_t level = node->node_level;
        
        // Initialize new level's repository when first node of a new level is encountered
        if (level != last_processed_level) {
            //printf("Processed Level %u :\n", level);
            seq_repo_init(&exist_repo[level], PER_LEVEL_GRAPH_NODES(SEQ_LENGTH_LIMIT, level));
            last_processed_level = level;
            // Merge parent level’s sequences into current level
            uint32_t parent_level = level - node->sequence_length;
            SequenceRepository* parent_repo = &exist_repo[parent_level];            
            for (uint32_t j = 0; j < parent_repo->capacity; j++) {
                if (parent_repo->is_used[j]) {
                    seq_repo_set_frequency(&exist_repo[level],
                                                parent_repo->entries[j].data,
                                                parent_repo->entries[j].length,
                                                parent_repo->values[j]);
                }
            }
#ifdef DEBUG
            printf("Initialized sequence repository for level %u, with capacity=%u\n", level, exist_repo[level].capacity);
#endif
        }

        // Add current node's sequence if length > 1
        if (node->sequence_length > 1) {
            seq_repo_increase_frequency(&exist_repo[level],
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


/**
 * Marks graph nodes as useless based on sequence frequency analysis.
 *
 * Step 1:
 * - A node is marked useless if its sequence appears only once in the entire graph.
 *
 * Step 2: A node is also useless if:
 *   - Its sequence appears exactly k times in the graph (2 <= k <= n_consec_level)
 *   - It does not appear at the parent level (freq is 0 at the parent level map)
 *   - And all k appearances are in consecutive levels
 *   - Marks ALL k matching nodes as useless.
 * 
 */
static void mark_nodes_useless(const uint8_t *block, uint8_t n_consec_level) {
    const uint16_t last_level = get_last_level_index();

    // Track candidates from last N levels
    typedef struct {
        uint32_t node_ids[SEQ_LENGTH_LIMIT];
        uint8_t count;
    } LevelCandidates;

    LevelCandidates level_candidates[MAX_CONSEC_LEVELS] = {0};
    uint8_t current_slot = 0;
    uint16_t current_level = 0;

    // Validate n_consec_level
    if (n_consec_level < 2 || n_consec_level > MAX_CONSEC_LEVELS) {
        fprintf(stderr, "Invalid n_consec_level: %d (must be 2-%d)\n",
                n_consec_level, MAX_CONSEC_LEVELS);
        return;
    }

#ifdef DEBUG
    printf("\nMarking useless nodes (n_consec_level=%d), graph size=%u\n",
           n_consec_level, graph.size);
#endif

    for (uint32_t i = 0; i < graph.size; i++) {
        GraphNode *node = &graph.nodes[i];

#ifdef DEBUG
        printf("Processing node=%u \n", node->node_id);
#endif
        if (node->sequence_length <= 1 || node->isUseless) {
            continue;
        }

        // Handle level transition
        if (node->node_level != current_level) {
            current_slot = (current_slot + 1) % n_consec_level;
            level_candidates[current_slot].count = 0;
            current_level = node->node_level;
        }

        const uint16_t parent_level = current_level - node->sequence_length;
        const uint8_t *seq = &block[node->offset];
        const uint8_t len = node->sequence_length;

        // Step 1: Mark unique sequences
        uint32_t freq_in_graph =
            seq_repo_get_frequency(&exist_repo[last_level], seq, len);
        if (freq_in_graph == 1) {
            node->isUseless = 1;
#ifdef DEBUG
            printf("Marked node_id=%u (unique sequence)\n", node->node_id);
#endif
            continue; // node is mark useless in step 1, no need to check for
                      // step 2.
        }

        // Step 2: Check for k-consecutive appearances (k = 2..n_consec_level)
        if (freq_in_graph >= 2 && freq_in_graph <= n_consec_level &&
            seq_repo_get_frequency(&exist_repo[parent_level], seq, len) == 0) {

            // Check all possible k values            
            uint32_t matches[MAX_CONSEC_LEVELS];
            uint8_t match_count = 1;
            matches[0] = node->node_id;

            // Check previous k-1 levels
            for (uint8_t offset = 1; offset < freq_in_graph; offset++) {
                uint8_t check_slot = (current_slot + n_consec_level - offset) % n_consec_level;

                for (uint8_t j = 0; j < level_candidates[check_slot].count; j++) {
                    GraphNode *candidate = get_graph_node(
                        level_candidates[check_slot].node_ids[j]);
                    if (!candidate->isUseless &&
                        candidate->sequence_length == len &&
                        sequences_equal(seq, &block[candidate->offset], len)) {
                        matches[match_count++] = candidate->node_id;
                        break;
                    }
                }
            }

            // If found k consecutive matches, mark them all
            if (match_count == freq_in_graph) {
                for (uint8_t m = 0; m < match_count; m++) {
                    GraphNode *matched = get_graph_node(matches[m]);
                    matched->isUseless = 1;
#ifdef DEBUG
                    printf("Marked node_id=%u (%d consecutive levels)\n",
                           matched->node_id, freq_in_graph);
#endif
                }               
            } 
        }

        // Add current node to candidates if not marked useless
        if (!node->isUseless &&
            level_candidates[current_slot].count < SEQ_LENGTH_LIMIT) {
            level_candidates[current_slot]
                .node_ids[level_candidates[current_slot].count++] =
                node->node_id;
        }
    }
}

void compact_graph(const uint8_t* block) {
    assert(graph.size == 0 || (graph.nodes[0].node_id == 0 && !graph.nodes[0].isUseless));    
  
    // Step 1: Analyze nodes and mark useless ones
    /*
    record_level_wize_nodes(block);
    printf("\n*** Done with creating level maps nodes=%u, in %lu ms\n",get_graph_size(), get_elapsed_ms());
    mark_nodes_useless(block, 10); 
    printf("\n*** Done with marking useless nodes=%u, in %lu ms\n",get_graph_size(), get_elapsed_ms());
    */
    uint32_t write_idx = 0;
    uint32_t level_start = 0;
    uint32_t current_level = 0;
    
#ifdef DEBUG
    printf("\n\nStarting compacting graph. \nTotal nodes before compaction =%u\n", graph.size);
#endif

    // Initialize root node
    graph.nodes[0].min_depth = 0;
    graph.level_min_depth[0] = 0;

    // Initialize first level's repository
    
    for (uint32_t read_idx = 0; read_idx < graph.size; read_idx++) {
        GraphNode* node = &graph.nodes[read_idx];
        // Calculate parent level
        uint16_t parent_level = node->node_level - node->sequence_length;
#ifdef DEBUG
        printf("\n\nProcessing node=%u, read_idx=%u, write_idx=%u\n", node->node_id, read_idx, write_idx);
#endif
        
        // Detect level transition
        if (node->node_level != current_level) {
            // Save the previous level's boundary
            graph.first_node_of_level[current_level] = level_start;
            
            // Update to new level
            current_level = node->node_level;
            level_start = write_idx;        
        }
        // Skip useless nodes
        if (node->isUseless) {
#ifdef DEBUG
            printf(" Ignoring useless node %u\n", node->node_id);
#endif            
            continue;
        }

        // Compact node to new position if needed
        if (write_idx != read_idx) {
            graph.nodes[write_idx] = *node;
        }

        GraphNode* new_node = &graph.nodes[write_idx];
        new_node->node_id = write_idx;
        
        
        
        // For non-root nodes, merge parent's sequences and calculate depth
        if (write_idx != 0) {
            // Validate parent level
            if (parent_level >= graph.total_levels) {
                fprintf(stderr, "Invalid parent level %u for node %u\n", 
                       parent_level, new_node->node_id);
                exit(1);
            }

            // Calculate node's minimum depth
            new_node->min_depth = graph.level_min_depth[parent_level] + 1;
        }

        // Update level's minimum depth
        if (write_idx == level_start) {
            // First node in level sets initial depth
            graph.level_min_depth[current_level] = new_node->min_depth;
        } else if (new_node->min_depth < graph.level_min_depth[current_level]) {
            // Subsequent nodes may lower the level's depth
            graph.level_min_depth[current_level] = new_node->min_depth;
        }

        write_idx++;
    }

    // Finalize last level's boundary
    graph.first_node_of_level[current_level] = level_start;

    // Initialize any remaining levels (if graph.total_levels was reduced)
    for (uint32_t l = current_level + 1; l < graph.total_levels; l++) {
        graph.first_node_of_level[l] = write_idx;
        graph.level_min_depth[l] = UINT16_MAX; // Mark as invalid
    }

    // Update graph size
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