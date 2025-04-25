// binseq_hashmap.h 
#ifndef BINSEQ_HASHMAP_H
#define BINSEQ_HASHMAP_H

#include <stdint.h>
#include <stddef.h>

// Opaque pointer to hide implementation details
typedef struct BinSeqMap BinSeqMap;

// Create/destroy functions
BinSeqMap* binseq_map_create(size_t initial_capacity);
void binseq_map_free(BinSeqMap* map);

// Map operations
int binseq_map_put(BinSeqMap* map, 
                  const uint8_t* key_sequence, uint16_t key_length,
                  int value_frequency);

const int* binseq_map_get_frequency(const BinSeqMap* map, 
                                   const uint8_t* key_sequence, uint16_t key_length);

int binseq_map_increment_frequency(BinSeqMap* map, 
                                 const uint8_t* key_sequence, uint16_t key_length);

// Utility functions
size_t binseq_map_size(const BinSeqMap* map);
size_t binseq_map_capacity(const BinSeqMap* map);
void binseq_map_print(const BinSeqMap* map);

// Special copy function for TreeNode (implementation can see TreeNode definition)
struct TreeNode;
int binseq_map_copy_to_node(const BinSeqMap* source, struct TreeNode* target);

#endif