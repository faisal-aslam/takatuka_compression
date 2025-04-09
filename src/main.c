#include "common_types.h"
#include "weighted_freq.h"
#include "xxhash.h"
#include "second_pass/second_pass.h"
#include "second_pass/binseq_hashmap.h"

#ifdef DEBUG
#include "debug/debug_sequences.h"
#define DEBUG_SEQUENCE_FILE "src/debug/debug_sequences.txt"
#endif

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <binary_file>\n", argv[0]);
        return 1;
    }
    
    processSecondPass(argv[1]);

    return 0;
}

