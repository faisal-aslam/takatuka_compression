#ifndef NEURALNET_GRAPH_H
#define NEURALNET_GRAPH_H

#include <stdint.h>
#include "../constants.h"

/**
 * Graph structure for neural network operations
 * Implements a multi-level graph where nodes can have weighted parent connections
 */

// Maximum levels in the graph (based on BLOCK_SIZE from constants)
#define MAX_LEVELS BLOCK_SIZE

// Maximum weights per node (based on sequence length limit)
#define MAX_WEIGHTS SEQ_LENGTH_LIMIT

/**
 * Represents a connection to a parent node
 * Contains parent ID, connection weight, and traversal cost
 */
typedef struct {
    uint32_t parent_id;    // Unique identifier of parent node
    uint8_t weight;        // Connection weight (0-255)
    uint8_t cost;         // Traversal cost for this connection
} ParentLink;

/**
 * Represents a node in the graph
 * Each node can have multiple parent connections
 */
typedef struct {
    uint32_t node_id;             // Unique graph identifier for this node
    ParentLink parent_link[MAX_WEIGHTS];  // Array of possible parent connections
    uint8_t parent_count;         // Current number of valid parent connections
} GraphNode;

/**
 * Graph structure containing all nodes organized by level
 */
typedef struct {
    GraphNode nodes[MAX_LEVELS][MAX_WEIGHTS];  // 2D array of nodes [level][weight]
    uint16_t current_level;       // Tracks the highest currently used level
} Graph;

// Initialize the graph (operates on internal static graph)
void init_graph(void);

// Get nodes at level (operates on internal static graph)
GraphNode* get_all_nodes_of_level(uint16_t level);

// Get current level (operates on internal static graph)
uint16_t get_current_level(void);

// Add parent link (operates on internal static graph)
void add_link_to_parent(GraphNode* child_node, GraphNode* parent_node, uint8_t weight, uint8_t cost);

// returns node of the graph based on node id.
GraphNode* get_graph_node(uint32_t node_id);

#endif // NEURALNET_GRAPH_H