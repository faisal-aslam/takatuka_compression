/*
typedef struct TreeNode {
    BinSeqMap *map;
    struct TreeNode *left, *right;
} TreeNode;

// Example usage
TreeNode *node1 = malloc(sizeof(TreeNode));
node1->map = binseq_map_create(1024);

TreeNode *node2 = malloc(sizeof(TreeNode));
node2->map = binseq_map_create(1024);

// Add entries to each independently
BinSeqKey key = { .binary_sequence = (uint8_t *)"1010", .length = 4 };
BinSeqValue val1 = { .frequency = 10, .weight = 1.5 };
BinSeqValue val2 = { .frequency = 20, .weight = 3.0 };

binseq_map_put(node1->map, key, val1);
binseq_map_put(node2->map, key, val2);

// Access independently
BinSeqValue *v1 = binseq_map_get(node1->map, key);
BinSeqValue *v2 = binseq_map_get(node2->map, key);

*/
