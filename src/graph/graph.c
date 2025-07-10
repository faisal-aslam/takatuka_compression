#include "graph.h"
#include "../map/seq_freq_map.h"
#include "timer.h"

#define MAX_CONSEC_LEVELS 10
#define PER_LEVEL_GRAPH_NODES(SEQ_LIMIT, LEVEL) \
    ((LEVEL) == 0 ? 1 : \
    ((LEVEL) <= (SEQ_LIMIT)) ? ((LEVEL) * ((LEVEL) + 1)) / 2 \
                             : ((SEQ_LIMIT) * ((SEQ_LIMIT) + 1)) / 2 + ((LEVEL) - (SEQ_LIMIT)) * (SEQ_LIMIT))

#define MIN_RLE_SEQ_LENGTH 6
#define RLE_MAX_PATTERN_LENGTH 3 // such as abcabcabc...

Graph graph; // Actual single definition

void init_graph(void) {
    graph.size = 0;
    graph.total_levels = 0;    
    memset(graph.first_node_of_level, 0, sizeof(graph.first_node_of_level));
}

/**
 * @brief Detects RLE-eligible sequences with optimized path for uniform sequences
 * 
 * Optimization strategy:
 * 1. Always check uniform sequences first (common case)
 * 2. Only check complex patterns when:
 *    - Sequence is sufficiently long
 *    - Not obviously random (based on first few bytes)
 * 3. Use efficient comparisons and early exits
 */
uint8_t is_RLE_sequence(GraphNode* node, const uint8_t *block) {
    // Initialize outputs
    node->run_length_encoding = 0;
    node->repeat_seq_length = 0;

    // Fast rejection for short sequences
    if (node->sequence_length < MIN_RLE_SEQ_LENGTH) {
        return 0;
    }

    const uint8_t* sequence = block + node->offset;
    const uint8_t first_byte = sequence[0];

    // ===== STAGE 1: Uniform Sequence Check =====
    bool uniform = true;
    // Unroll first 4 bytes for quick rejection
    if (sequence[1] != first_byte || sequence[2] != first_byte || sequence[3] != first_byte) {
        uniform = false;
    } else {
        // Only check remaining bytes if first 4 matched
        for (uint8_t i = 4; i < node->sequence_length; i++) {
            if (sequence[i] != first_byte) {
                uniform = false;
                break;
            }
        }
    }

    if (uniform) {
        node->run_length_encoding = 1;
        node->repeat_seq_length = 1;
        
        #ifdef DEBUG
        printf("[RLE] Uniform: ID=%u @%u (len=%u)\n", 
               node->node_id, node->offset, node->sequence_length);
        print_node_sequence(node, block);
        #endif
        return 1;
    }

    // ===== STAGE 2: Non-uniform Pattern Check =====
    // Only check patterns if sequence is long enough to justify the cost
    if (node->sequence_length < 16) {  // Threshold adjustable based on profiling
        return 0;
    }

    // Quick entropy check - if first 4 bytes are unique, unlikely to have patterns
    uint8_t unique_bytes = 0;
    for (uint8_t i = 0; i < 4; i++) {
        if (i == 0 || sequence[i] != sequence[i-1]) {
            unique_bytes++;
        }
    }
    if (unique_bytes == 4) {  // All first 4 bytes different
        return 0;
    }

    uint8_t max_pattern = MIN(node->sequence_length / 2, RLE_MAX_PATTERN_LENGTH);

    // Check from largest possible pattern down
    for (uint8_t pattern_len = max_pattern; pattern_len >= 2; pattern_len--) {
        if (node->sequence_length % pattern_len != 0) continue;

        uint8_t repeats = node->sequence_length / pattern_len;
        bool valid = true;

        // Compare pattern segments
        for (uint8_t r = 1; r < repeats; r++) {
            if (memcmp(sequence, sequence + r * pattern_len, pattern_len) != 0) {
                valid = false;
                break;
            }
        }

        if (valid && repeats >= 2) {  // Require at least 2 full repeats
            node->run_length_encoding = 1;
            node->repeat_seq_length = pattern_len;
            
            #ifdef DEBUG
            printf("[RLE] Pattern: ID=%u @%u (len=%u) [pattern=%u repeats=%u]\n",
                   node->node_id, node->offset, node->sequence_length,
                   pattern_len, repeats);
            print_node_sequence(node, block);
            #endif
            return 1;
        }
    }

    return 0;
}
// Verification function of the graph.
static void verify_graph_integrity(const uint8_t *block) {
    uint8_t beggining_of_last_level = 0;
    for (uint32_t i = 0; i < graph.size; i++) {
        GraphNode *node = &graph.nodes[i];
        if (graph.first_node_of_level[node->node_level] <
            beggining_of_last_level) {
            fprintf(stderr, "Illegal level boundaries at level=%u\n",
                    node->node_level);
            abort();
        }
        beggining_of_last_level = graph.first_node_of_level[node->node_level];
        if (node->isUseless) {
            fprintf(stderr,
                    "\nStill found a useless node. There should be None. "
                    "node_id=%u, node_level=%u \n\n",
                    node->node_id, node->node_level);
            abort();
        }
        if (node->node_id != 0) {
            uint16_t parent_level = node->node_level - node->sequence_length;
            uint16_t parent_node_count = get_parent_nodes_count(node);
            if (parent_level >= graph.total_levels || parent_node_count > SEQ_LENGTH_LIMIT) {
                fprintf(stderr, "Node %u has invalid parent level %u, with node_count=%u\n",
                        node->node_id, parent_level, parent_node_count);
                abort();
            }
        }
        //print_graph_node(node);
        //print_node_sequence(node, block);
        //printf("\n");
    }
}

static inline void calculate_levels_to_keep(uint8_t *levels_to_keep) {
    // Clear all levels (0 = don't keep)
    memset(levels_to_keep, 0, graph.total_levels * sizeof(uint8_t));
    
    // Keep the last and the first level by default
    if (graph.total_levels > 0) {
        levels_to_keep[graph.total_levels - 1] = 1;
        levels_to_keep[0] = 1;
    }

    uint16_t current_level = graph.total_levels-1; //starting from the last level.
    uint8_t has_useful_node = 0;
    // Process nodes in reverse order
    for (int32_t read_idx = graph.size - 1; read_idx >= 0; read_idx--) {
        GraphNode* node = &graph.nodes[read_idx];
        if (current_level != node->node_level) {
            if (!has_useful_node && levels_to_keep[current_level]) {
                uint32_t first_node_id = graph.first_node_of_level[current_level];
                GraphNode* firstNode = get_graph_node(first_node_id);
                firstNode->isUseless = 0; //make it useful as every good level must have one.
                uint16_t parent_level = get_parent_level(firstNode);
                if (parent_level < graph.total_levels) {  // Validate parent level
                    levels_to_keep[parent_level] = 1;
                }
            }
            has_useful_node = 0; //mark it zero at the begining of every level.
            current_level = node->node_level;
        }
        // Only process nodes from levels we're keeping
        if (!levels_to_keep[node->node_level]) {
            continue;
        }

        // Mark parent level to keep (if not root node)
        if (node->node_id != 0 && !node->isUseless) {  // Skip root node (level 0)
            has_useful_node = 1;
            uint16_t parent_level = get_parent_level(node);
            if (parent_level < graph.total_levels) {  // Validate parent level
                levels_to_keep[parent_level] = 1;
            }
        }
    }
}

void compact_graph(const uint8_t *block) {
    assert(graph.size == 0 ||
           (graph.nodes[0].node_id == 0 && !graph.nodes[0].isUseless));

    uint32_t write_idx = 0;
    uint32_t current_level = 0;
    uint32_t level_start = 0;
    uint32_t max_level_processed = 0; // Track the highest level we actually process
    uint8_t levels_to_keep[graph.total_levels];
    calculate_levels_to_keep(levels_to_keep);
    // Initialize root node
    if (graph.size > 0) {
        graph.nodes[0].min_depth = 0;
        graph.level_min_depth[0] = 0;
    }

    for (uint32_t read_idx = 0; read_idx < graph.size; read_idx++) {
        GraphNode *node = &graph.nodes[read_idx];
#ifdef DEBUG
        printf("\nCompacting node=%u\n", node->node_id);
#endif
        // Level transition handling
        if (node->node_level != current_level) {
            // Finalize previous level
            if (levels_to_keep[current_level] == 1) {              
                graph.first_node_of_level[current_level] = level_start;
            } else {
                graph.first_node_of_level[current_level] = UINT32_MAX;
            }

            // Update max processed level
            if (current_level > max_level_processed) {
                max_level_processed = current_level;
            }

            // Start new level
            current_level = node->node_level;
            level_start = write_idx;
        }

        // Skip nodes from excluded levels. Skip useless nodes too, unless we force-include them.
        if (levels_to_keep[current_level] == 0 || node->isUseless) {
#ifdef DEBUG
        printf("\nNode Excluded=%u, level=%u\n", node->node_id, node->node_level);
#endif

            continue;
        }

        // Copy node if needed and update properties
        if (write_idx != read_idx) {
            graph.nodes[write_idx] = *node;
        }
        GraphNode *new_node = &graph.nodes[write_idx];
        new_node->node_id = write_idx;

        // Calculate depth (root has depth 0)
        if (write_idx != 0) {
            uint16_t parent_level =
                new_node->node_level - new_node->sequence_length;
            assert(parent_level < graph.total_levels);
            new_node->min_depth = graph.level_min_depth[parent_level] + 1;
        }

        // Update level's min depth
        if (write_idx == level_start) {
            graph.level_min_depth[current_level] = new_node->min_depth;
        } else {
            graph.level_min_depth[current_level] =
                MIN(graph.level_min_depth[current_level], new_node->min_depth);
        }
#ifdef DEBUG
        printf("\nNode included=%u, level=%u\n", node->node_id, node->node_level);
#endif

        write_idx++;
    }

    // Finalize last level
    graph.first_node_of_level[current_level] = level_start;


#ifdef DEBUG
        printf("\nGraph size before compaction %u and after =%u\n", graph.size, write_idx);
#endif

    graph.size = write_idx;
#ifdef DEBUG
    verify_graph_integrity(block);
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
    printf("\n");
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