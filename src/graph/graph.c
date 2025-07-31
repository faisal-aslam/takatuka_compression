#include "graph.h"
#include "seq_freq_map.h"
#include "timer.h"
#include <assert.h>

#define MAX_CONSEC_LEVELS 10
#define PER_LEVEL_GRAPH_NODES(SEQ_LIMIT, LEVEL)                                                                        \
    ((LEVEL) == 0               ? 1                                                                                    \
     : ((LEVEL) <= (SEQ_LIMIT)) ? ((LEVEL) * ((LEVEL) + 1)) / 2                                                        \
                                : ((SEQ_LIMIT) * ((SEQ_LIMIT) + 1)) / 2 + ((LEVEL) - (SEQ_LIMIT)) * (SEQ_LIMIT))

#define MIN_RLE_SEQ_LENGTH 10
#define RLE_MAX_PATTERN_LENGTH 3 // such as abcabcabc...

Graph graph; // Actual single definition

void compact_graph(const uint8_t *block) {
    assert(graph.size == 0 || (graph.nodes[0].node_id == 0 && !graph.nodes[0].useless));

    uint32_t write_idx = 0;
    uint32_t current_level = 0;
    uint32_t level_start = 0;
    uint32_t max_level_processed = 0; // Track the highest level we actually process
    // calculate_levels_to_keep(levels_to_keep);
    //  Initialize root node
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

            graph.first_node_of_level[current_level] = level_start;

            // Update max processed level
            if (current_level > max_level_processed) {
                max_level_processed = current_level;
            }

            // Start new level
            current_level = node->node_level;
            level_start = write_idx;
        }

        if (node->useless) {
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
            uint16_t parent_level = new_node->node_level - new_node->sequence_length;
            assert(parent_level < graph.total_levels);
            new_node->min_depth = graph.level_min_depth[parent_level] + 1;
        }

        // Update level's min depth
        if (write_idx == level_start) {
            graph.level_min_depth[current_level] = new_node->min_depth;
        } else {
            graph.level_min_depth[current_level] = MIN(graph.level_min_depth[current_level], new_node->min_depth);
        }
#ifdef DEBUG
        printf("\nNode included=%u, level=%u\n", node->node_id, node->node_level);
#endif

        write_idx++;
    }

    // Finalize last level
    graph.first_node_of_level[current_level] = level_start;

    // #ifdef DEBUG
    printf("\nGraph size before compaction %u and after =%u\n", graph.size, write_idx);
    // #endif

    graph.size = write_idx;
}

void init_graph(void) {
    graph.size = 0;
    graph.total_levels = 0;
    memset(graph.first_node_of_level, 0, sizeof(graph.first_node_of_level));
}

void mass_increment_levels(int add_levels) {
    if (graph.total_levels + add_levels < MAX_LEVELS) {
        for (int i = 0; i < add_levels; i++) {
            graph.first_node_of_level[graph.total_levels + i] = UINT32_MAX; // no node at this level.
        }
        graph.total_levels += add_levels;
    }
}

uint8_t is_RLE_sequence(uint8_t *repeat_seq_length, uint8_t *length_of_RLE, uint8_t block_size, uint32_t offset,
                        const uint8_t *block) {
    *repeat_seq_length = 0;
    *length_of_RLE = 0;

    if (block_size < MIN_RLE_SEQ_LENGTH) {
        return 0;
    }

    const uint8_t *sequence = block + offset;
    const uint8_t first_byte = sequence[0];

    // ===== Stage 1: Uniform Sequence Check (for whole sequence or prefix) =====
    uint8_t uniform_length = block_size;

    // Find the first position where the byte differs
    for (uint8_t i = 1; i < block_size; i++) {
        if (sequence[i] != first_byte) {
            uniform_length = i;
            break;
        }
    }

    if (uniform_length >= MIN_RLE_SEQ_LENGTH) {
        *repeat_seq_length = 1;
        *length_of_RLE = uniform_length;

#ifdef DEBUG
        printf("[RLE] Uniform sequence found at offset %u: repeat_len = %u, RLE_len = %u\n", offset, *repeat_seq_length,
               *length_of_RLE);
#endif

        return 1;
    }

    // ===== Stage 2: Pattern-Based RLE Check (for whole sequence or prefix) =====
    if (block_size < 16) {
        return 0;
    }

    // Quick entropy filter: check uniqueness among first 4 bytes
    bool is_unique = true;
    for (int i = 0; i < 4 && is_unique; i++) {
        for (int j = i + 1; j < 4; j++) {
            if (sequence[i] == sequence[j]) {
                is_unique = false;
                break;
            }
        }
    }
    if (is_unique) {
        return 0;
    }

    int max_pattern = MIN(block_size / 2, RLE_MAX_PATTERN_LENGTH);

    for (int pattern_len = max_pattern; pattern_len >= 2; pattern_len--) {
        int max_valid_repeats = 1; // start with 1 pattern already seen
        bool valid = true;

        while (valid) {
            int base = (max_valid_repeats - 1) * pattern_len;
            int next = base + pattern_len;
            if ((uint32_t)(next + pattern_len) > (block_size - offset)) {
                break;
            }

            for (int i = 0; i < pattern_len; i++) {
                if (sequence[base + i] != sequence[next + i]) {
                    valid = false;
                    break;
                }
            }

            if (valid) {
                max_valid_repeats++;
            }
        }

        int total_length = max_valid_repeats * pattern_len;

        if (max_valid_repeats >= 2 && total_length >= MIN_RLE_SEQ_LENGTH) {
            *repeat_seq_length = pattern_len;
            *length_of_RLE = total_length;

#ifdef DEBUG
            printf("[RLE] Pattern sequence found at offset %u: repeat_len = %u, RLE_len = %u, repeats = %d\n", offset,
                   *repeat_seq_length, *length_of_RLE, max_valid_repeats);
#endif

            return 1;
        }
    }

    return 0;
}

static inline void print_node_link(GraphNode *node, GraphNode *parent) {
    printf("\t %u --> %u\n", node->node_id, parent->node_id);
}

void print_node_sequence(GraphNode *node, const uint8_t *block) {
    for (int i = 0; i < node->sequence_length; i++) {
        printf("%c", block[node->offset + i]);
        if (i + 1 < node->sequence_length) {
            // printf(",");
        }
    }
}

void print_graph_node(GraphNode *node) {
    if (!node) return;

    if (node->node_id == 0) {
        printf("\nROOT NODE ");
    } else {
        printf("\n");
    }
    uint16_t parent_nodes_count = get_parent_nodes_count(node);
    printf("node_id = %u, start_of_sequence = %u, sequence_length = %u, level=%u", node->node_id, node->offset,
           node->sequence_length, node->node_level);
    if (!node->is_RLE) {
        printf(", parent_count = %u\n", parent_nodes_count);
    } else {
        printf(", RLE=YES, parent_count = %u\n", parent_nodes_count);
    }
    GraphNode *parent_nodes = get_parent_nodes(node);
    if (!parent_nodes) return;
    for (int i = 0; i < parent_nodes_count; i++) {
        print_node_link(node, &parent_nodes[i]);
        break; // just print one link per node as other belings to the same level.
    }
}

void print_all_nodes(const uint8_t *block) {
    for (uint32_t i = 0; i < graph.size; i++) {
        GraphNode *node = &graph.nodes[i];
        print_graph_node(node);
        print_node_sequence(node, block);
    }
}