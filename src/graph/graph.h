// graph/graph.h

#ifndef GRAPH_NODE_H
#define GRAPH_NODE_H

#include "../constants.h"
#include <stdint.h>
#include <stdbool.h>

// Graph configuration constants
#define MAX_LEVELS BLOCK_SIZE  // Maximum number of levels in the graph
#define GRAPH_MAX_NODES ((MAX_LEVELS)*(SEQ_LENGTH_LIMIT*SEQ_LENGTH_LIMIT))    // Maximum number of nodes in the graph

// Compile-time assertion macro for different C standards
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
#define STATIC_ASSERT(cond, msg) typedef char static_assert_##msg[(cond) ? 1 : -1]
#endif


// To track the first node at a level with a given weight
typedef struct {
    uint32_t first_node_with_weight; // Tracks first node with each weight at current level
    uint8_t weight;
} WeightTracker;

typedef struct {    
    uint32_t parent_node_id;    
    uint32_t saving_so_far;       // Compression savings up to this node
    int32_t map_index;
} ParentLink;


// Graph node structure representing a node in the graph
typedef struct {
    uint32_t id;                 // Unique identifier for the node
    uint32_t compress_start_index; // Start index in the original data block    
    ParentLink parent_links[SEQ_LENGTH_LIMIT];  // Array of parent node IDs
    uint16_t level;              // Level of this node in the graph hierarchy
    uint8_t parent_link_count;        // Number of parent nodes    
    uint8_t incoming_weight;     // Weight/sequence length associated with this node    
    uint8_t compress_sequence_length;   // Length of the sequence this node represents        
} GraphNode;



// Structure representing a slot for nodes with specific weight and level
typedef struct {
    uint32_t indices[SEQ_LENGTH_LIMIT];  // Array of node indices
    uint8_t count;                       // Number of nodes in this slot
} WeightLevelSlot;

// Index structure to organize nodes by weight and level
typedef struct {
    //array of level and weight.
    WeightLevelSlot slots[MAX_LEVELS][SEQ_LENGTH_LIMIT]; // 2D array of slots
    uint16_t max_level;         // Current maximum level in graph
} GraphIndex;

// Main graph structure containing all nodes and indexing
typedef struct {
    GraphNode nodes[GRAPH_MAX_NODES];      // Array of all graph nodes
    GraphIndex index;                      // Weight/level index structure
    uint32_t current_node_index;          // Next available node index
    WeightTracker weight_cache[SEQ_LENGTH_LIMIT]; //Record first node of each weight.
    bool initialized;                     // Flag indicating if graph is initialized
} Graph;

// Compile-time assertions to verify structure sizes and limits
//STATIC_ASSERT(sizeof(GraphNode) == 18 + (8 * SEQ_LENGTH_LIMIT), "GraphNode size mismatch");
STATIC_ASSERT(SEQ_LENGTH_LIMIT <= 255, "SEQ_LENGTH_LIMIT too large");
STATIC_ASSERT(SEQ_LENGTH_LIMIT > 0, "SEQ_LENGTH_LIMIT too small");

// Global graph instance
extern Graph graph;

// Function declarations
void graph_init(void);  // Initialize the graph structure
GraphNode* graph_get_node(uint32_t index);  // Get node by index
bool graph_add_parent_edge(GraphNode* child_node, GraphNode* parent_node, const uint8_t* block);   // Add parent edge from a child to its parent.
GraphNode* create_new_node(uint8_t weight, uint32_t level);  // Create new node
bool is_graph_full(void);  // Check if graph is at maximum capacity
void print_graph_node(const GraphNode *node, const uint8_t* block);  // Print node info
uint32_t get_current_graph_node_index(void);  // Get current node index
const uint32_t* get_nodes_by_weight_and_level(uint8_t weight, uint32_t level, uint32_t* count);  // Query nodes by weight/level
uint32_t get_max_level(void);  // Get maximum level in graph
GraphNode* get_first_node_of_last_level(void); // returns the first node of the last level of the graph. This is use to create maps.
#endif