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

void compact_graph(const uint8_t* block) {
    assert(graph.size == 0 || (graph.nodes[0].node_id == 0 && !graph.nodes[0].isUseless));    
  
    uint32_t write_idx = 0;
    uint32_t level_start = 0;
    uint32_t current_level = 0;
    bool level_has_useful_node = false;
    uint32_t first_node_in_level_idx = 0;
    
#ifdef DEBUG
    printf("\n\nStarting compacting graph. \nTotal nodes before compaction =%u\n", graph.size);
#endif

    // Initialize root node
    graph.nodes[0].min_depth = 0;
    graph.level_min_depth[0] = 0;

    for (uint32_t read_idx = 0; read_idx < graph.size; ) {
        GraphNode* node = &graph.nodes[read_idx];
        
        // Detect level transition
        if (node->node_level != current_level) {
            // Check if previous level had any useful nodes
            if (current_level > 0 && !level_has_useful_node) {
                // Forcefully mark the first node of the level as useful
                graph.nodes[first_node_in_level_idx].isUseless = 0;
                // Jump back to process this node
                read_idx = first_node_in_level_idx;
#ifdef DEBUG
                printf(" Level %u had no useful nodes - marking node %u as useful\n", 
                       current_level, first_node_in_level_idx);
#endif
                // Reset level tracking for reprocessing
                current_level = node->node_level;
                level_has_useful_node = false;
                continue;
            }
            
            // Save the previous level's boundary
            graph.first_node_of_level[current_level] = level_start;
            
            // Update to new level
            current_level = node->node_level;
            level_start = write_idx;
            level_has_useful_node = false;
            first_node_in_level_idx = read_idx;
        }
        
        // Skip useless nodes (unless we're processing a forced useful node)
        if (node->isUseless) {
#ifdef DEBUG
            printf(" Ignoring useless node %u\n", node->node_id);
#endif            
            read_idx++;
            continue;
        }

        level_has_useful_node = true;
        
        // Compact node to new position if needed
        if (write_idx != read_idx) {
            graph.nodes[write_idx] = *node;
        }

        GraphNode* new_node = &graph.nodes[write_idx];
        new_node->node_id = write_idx;
        
        // For non-root nodes, merge parent's sequences and calculate depth
        if (write_idx != 0) {
            // Calculate parent level
            uint16_t parent_level = node->node_level - node->sequence_length;
            
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
        read_idx++;
    }

    // Check if the last level had any useful nodes
    if (!level_has_useful_node && first_node_in_level_idx < graph.size) {
        // Forcefully mark the first node of the level as useful
        graph.nodes[first_node_in_level_idx].isUseless = 0;
        // Process this node
        GraphNode* node = &graph.nodes[first_node_in_level_idx];
        
        if (write_idx != first_node_in_level_idx) {
            graph.nodes[write_idx] = *node;
        }

        GraphNode* new_node = &graph.nodes[write_idx];
        new_node->node_id = write_idx;
        
        if (write_idx != 0) {
            uint16_t parent_level = node->node_level - node->sequence_length;
            if (parent_level >= graph.total_levels) {
                fprintf(stderr, "Invalid parent level %u for node %u\n", 
                       parent_level, new_node->node_id);
                exit(1);
            }
            new_node->min_depth = graph.level_min_depth[parent_level] + 1;
        }

        if (write_idx == level_start) {
            graph.level_min_depth[current_level] = new_node->min_depth;
        } else if (new_node->min_depth < graph.level_min_depth[current_level]) {
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