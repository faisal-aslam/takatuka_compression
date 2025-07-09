#include "graph.h"
#include "../map/seq_freq_map.h"
#include "timer.h"

#define MAX_CONSEC_LEVELS 10
#define PER_LEVEL_GRAPH_NODES(SEQ_LIMIT, LEVEL) \
    ((LEVEL) == 0 ? 1 : \
    ((LEVEL) <= (SEQ_LIMIT)) ? ((LEVEL) * ((LEVEL) + 1)) / 2 \
                             : ((SEQ_LIMIT) * ((SEQ_LIMIT) + 1)) / 2 + ((LEVEL) - (SEQ_LIMIT)) * (SEQ_LIMIT))


Graph graph; // Actual single definition

void init_graph(void) {
    graph.size = 0;
    graph.total_levels = 0;    
    memset(graph.first_node_of_level, 0, sizeof(graph.first_node_of_level));
}

// Verification function of the graph.
void verify_graph_integrity(const uint8_t *block) {
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

void compact_graph(const uint8_t *block, uint8_t *levels_to_keep) {
    assert(graph.size == 0 ||
           (graph.nodes[0].node_id == 0 && !graph.nodes[0].isUseless));

    uint32_t write_idx = 0;
    uint32_t current_level = 0;
    uint32_t level_start = 0;
    uint32_t first_node_idx = 0;
    bool level_has_useful = false;
    uint32_t max_level_processed = 0; // Track the highest level we actually process

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
                if (!level_has_useful) {
                    // Force include first node
                    GraphNode *first_node = &graph.nodes[first_node_idx];
                    first_node->isUseless = 0;
                    read_idx = first_node_idx - 1;
                    current_level = first_node->node_level;
                    level_start = write_idx;
                    level_has_useful = false;
                    continue;
                }
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
            first_node_idx = read_idx;
            level_start = write_idx;
            level_has_useful = false;
        }

        // Skip nodes from excluded levels. Skip useless nodes too, unless we force-include them.
        if (levels_to_keep[current_level] == 0 || node->isUseless) {
#ifdef DEBUG
        printf("\nNode Excluded=%u, level=%u\n", node->node_id, node->node_level);
#endif

            continue;
        }

        // Mark level as having useful nodes
        level_has_useful = true;

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
    if (current_level < graph.total_levels) {
        if (levels_to_keep[current_level] == 1) {
            if (!level_has_useful) {
                // Force include first node of last level
                GraphNode *first_node = &graph.nodes[first_node_idx];
                first_node->isUseless = 0;

                if (write_idx != first_node_idx) {
                    graph.nodes[write_idx] = *first_node;
                }

                GraphNode *new_node = &graph.nodes[write_idx];
                new_node->node_id = write_idx;

                if (write_idx != 0) {
                    uint16_t parent_level =
                        new_node->node_level - new_node->sequence_length;
                    new_node->min_depth =
                        graph.level_min_depth[parent_level] + 1;
                }

                graph.first_node_of_level[current_level] = write_idx;
                graph.level_min_depth[current_level] = new_node->min_depth;
                write_idx++;
            } else {
                graph.first_node_of_level[current_level] = level_start;
            }
        } else {
            graph.first_node_of_level[current_level] = UINT32_MAX;
        }

        // Update max processed level one last time
        if (current_level > max_level_processed) {
            max_level_processed = current_level;
        }
    }
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