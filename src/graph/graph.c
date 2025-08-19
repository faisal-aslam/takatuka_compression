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

static void compact_levels(const uint8_t* block) {    
    for (uint32_t l = 0; l < graph.total_levels; l++) {
        uint32_t start_level_id = get_level_start_id(l);
        uint32_t end_level_id = get_level_end_id(l);
        uint32_t prevFreq;
        for (uint32_t id=start_level_id+1; id < end_level_id; id++) {
            GraphNode *node = get_graph_node(id);
            if (node->useless) continue;
            uint32_t freq, dummy_node_id;
            seq_freq_get(&block[node->offset], node->sequence_length, &freq, &dummy_node_id);
            if (id != start_level_id+1 && prevFreq <= freq) {                            
                //make previous node useless.
                //we have found a larger combination with same or more freq.
                GraphNode *pre_node = get_graph_node(id-1);
                pre_node->useless = 1;
            }
            prevFreq = freq;
        }
    }

}

void compact_graph(const uint8_t *block) {
    (void)block; // Mark as intentionally unused
    if (graph.size == 0) return;

    uint32_t write_idx = 0;
    uint32_t current_level = 0;
    //init_seq_freq_map();
    //compact_levels(block);
    // Pre-process: mark all levels as invalid initially
    for (uint32_t l = 0; l < graph.total_levels; l++) {
        graph.first_node_of_level[l] = UINT32_MAX;
    }

    // Process root node
    graph.first_node_of_level[0] = 0;
    write_idx = 1;

    // Main compaction loop
    for (uint32_t read_idx = 1; read_idx < graph.size; read_idx++) {
        GraphNode *node = &graph.nodes[read_idx];
        
        // Fast path: skip useless nodes immediately
        if (node->useless) continue;

        // Handle level transitions
        if (node->node_level > current_level) {
            // Update all empty levels between current and node's level
            for (uint32_t l = current_level + 1; l <= node->node_level; l++) {
                graph.first_node_of_level[l] = write_idx;
            }
            current_level = node->node_level;
        }

        // Copy node (use memmove if overlapping is possible)
        graph.nodes[write_idx] = *node;
        GraphNode *new_node = &graph.nodes[write_idx];
        //seq_freq_increment(&block[new_node->offset], new_node->sequence_length, 1);
        new_node->node_id = write_idx;


        write_idx++;
    }

    // Finalize graph metadata
    printf("%lu: Done with graph compaction from %u to %u nodes\n",get_elapsed_ms(), graph.size, write_idx);
    graph.size = write_idx;
    graph.total_levels = current_level + 1;
    seq_freq_map_print();

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
    if (1) return 0; //not supporting multiple byte pattern.
    
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