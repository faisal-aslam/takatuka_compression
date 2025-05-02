//write_in_file.h
#ifndef WRITE_IN_FILE_H
#define WRITE_IN_FILE_H

#include "common_types.h"
#include "../second_pass/group.h"
#include "../second_pass/tree_node.h"

void writeCompressedOutput(const char* filename, BinarySequence** sequences, 
    int seq_count, TreeNode *best_node, const uint8_t* block);
                          
                          
#endif
