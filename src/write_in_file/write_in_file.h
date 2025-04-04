//write_in_file.h
#ifndef WRITE_IN_FILE_H
#define WRITE_IN_FILE_H

void writeCompressedOutput(const char* filename, BinarySequence* sequences, 
                          int seq_count, TreeNode *best_node, uint8_t* raw_data);
                          
                          
#endif
