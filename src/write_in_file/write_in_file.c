#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h> 
#include "write_in_file.h"
#include "code_classes.h"
#include "best_path_view.h"
#include "compressed_header.h"
#include "../map/code_map.h"
#include "compressed_body.h"
/**
  * @brief Main function to write complete compressed output file
  * 
  * @param filename Output file path
  * @param sequences Array of binary sequences for header
  * @param seq_count Number of sequences
  * @param best_node TreeNode with best compression path
  * @param block Pointer to raw data block
  */
void writeCompressedOutput(const char* filename, const uint8_t* block) {
    if (!filename || !block) {
        fprintf(stderr, "Error: Invalid inputs in writeCompressedOutput\n");
        return;
    }

    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open output file");
        return;
    }
    BestPathView best_view = get_best_path_view(); //we got the best view.
#ifdef DEBUG
    print_best_view(&best_view, 1, block); //to check if our view is consistent with the path computed.
#endif

    printf("\n ==== Starting compressed output writing === \n");

    //populate header by giving shorter code to greater saving sequences.
    populate_header(best_view, block, file);
    
    //populate body of the compressed file.
    populate_body(best_view, block, file);

	//printNode(best_node, raw_data, 0);
    /*int used_count = calcUsedAndAssignGroupID(block, 0);
    writeHeaderOfCompressedFile(sequences, seq_count, used_count, file);
    writeCompressedDataInFile(best_node, block, file);*/
    printf("\n === Writing compressed output completed ==\n");
   
    if (fclose(file) != 0) {
        perror("Warning: Error closing output file");
    }
    //free_code_map(code_map); 
}

