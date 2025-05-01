#include "second_pass/second_pass.h"
#include "stdio.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <binary_file>\n", argv[0]);
        return 1;
    }
    
    processSecondPass(argv[1]);

    return 0;
}
