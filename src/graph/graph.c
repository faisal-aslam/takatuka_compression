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

void compact_graph(const uint8_t* block, uint16_t* levels_to_keep) {
    assert(graph.size == 0 || (graph.nodes[0].node_id == 0 && !graph.nodes[0].isUseless));    

    uint32_t write_idx = 0;
    uint32_t level_start = 0;
    uint32_t current_level = 0;
    bool level_has_useful_node = false;
    uint32_t first_node_in_level_idx = 0;

#ifdef DEBUG
    printf("\n\nStarting compacting graph. \nTotal nodes before compaction = %u\n", graph.size);
#endif

    // Initialize root node
    graph.nodes[0].min_depth = 0;
    graph.level_min_depth[0] = 0;

    uint32_t read_idx = 0;
    while (read_idx < graph.size) {
        GraphNode* node = &graph.nodes[read_idx];

        // Level transition
        if (node->node_level != current_level) {
            // Finalize the previous level
            if (current_level > 0) {
                if (level_has_useful_node) {
                    graph.first_node_of_level[current_level] = level_start;
                } else {
                    // Forcefully mark the first node as useful and reprocess
                    GraphNode* first_node = &graph.nodes[first_node_in_level_idx];
                    first_node->isUseless = 0;
                    first_node->node_level = current_level--;
                    read_idx = first_node_in_level_idx;

#ifdef DEBUG
                    printf("Level %u had no useful nodes - force-keeping node %u\n", current_level, first_node_in_level_idx);
                    printf("Reprocessing level %u\n", current_level);
#endif
                    continue;
                }
            }

            // Prepare for new level
            current_level = node->node_level;
            level_start = write_idx;
            level_has_useful_node = false;
            first_node_in_level_idx = read_idx;
        }

        // Skip levels that are not marked to be kept
        if (levels_to_keep[node->node_level] == 0) {
            read_idx++;
            continue;
        }

        // Skip useless nodes
        if (node->isUseless) {
#ifdef DEBUG
            printf("Ignoring useless node %u at level %u\n", node->node_id, node->node_level);
#endif
            read_idx++;
            continue;
        }

        // Mark that this level has at least one useful node
        level_has_useful_node = true;

        // Copy node if necessary
        if (write_idx != read_idx) {
            graph.nodes[write_idx] = *node;
        }

        GraphNode* new_node = &graph.nodes[write_idx];
        new_node->node_id = write_idx;

        if (write_idx != 0) {
            uint16_t parent_level = new_node->node_level - new_node->sequence_length;
            if (parent_level >= graph.total_levels) {
                fprintf(stderr, "Invalid parent level %u for node %u\n", parent_level, new_node->node_id);
                exit(1);
            }

            new_node->min_depth = graph.level_min_depth[parent_level] + 1;
        }

        // Update min depth for this level
        if (write_idx == level_start) {
            graph.level_min_depth[current_level] = new_node->min_depth;
        } else if (new_node->min_depth < graph.level_min_depth[current_level]) {
            graph.level_min_depth[current_level] = new_node->min_depth;
        }

#ifdef DEBUG
        printf("\nNode_id=%u, node_level=%u, node_depth=%u\n", new_node->node_id, new_node->node_level, new_node->min_depth);
        print_node_sequence(new_node, block);
        printf("\n");
#endif

        write_idx++;
        read_idx++;
    }

    // Finalize the last level
    if (level_has_useful_node) {
        graph.first_node_of_level[current_level] = level_start;
    } else if (first_node_in_level_idx < graph.size) {
        // Forcefully mark the node as useful
        GraphNode* node = &graph.nodes[first_node_in_level_idx];
        node->isUseless = 0;

        if (write_idx != first_node_in_level_idx) {
            graph.nodes[write_idx] = *node;
        }

        GraphNode* new_node = &graph.nodes[write_idx];
        new_node->node_id = write_idx;

        if (write_idx != 0) {
            uint16_t parent_level = new_node->node_level - new_node->sequence_length;
            if (parent_level >= graph.total_levels) {
                fprintf(stderr, "Invalid parent level %u for node %u\n", parent_level, new_node->node_id);
                exit(1);
            }
            new_node->min_depth = graph.level_min_depth[parent_level] + 1;
        }

        graph.first_node_of_level[current_level] = write_idx;
        graph.level_min_depth[current_level] = new_node->min_depth;

        write_idx++;
    } else {
        graph.first_node_of_level[current_level] = UINT32_MAX;
    }

    // Mark unused levels beyond the last one as invalid
    for (uint32_t l = current_level + 1; l < graph.total_levels; l++) {
        graph.first_node_of_level[l] = UINT32_MAX;
        graph.level_min_depth[l] = UINT16_MAX;
    }

    graph.size = write_idx;

#ifdef DEBUG
    printf("Total nodes after compaction = %u\n", graph.size);
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