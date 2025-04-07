#ifndef CONSTANTS_H
#define CONSTANTS_H

#define SEQ_LENGTH_START 2
#define SEQ_LENGTH_LIMIT 5 //a.k.a k
#define BLOCK_SIZE (1 << 20)
#define LEAST_REDUCTION 16

// Group definitions
#define GROUP1_THRESHOLD 16
#define GROUP2_THRESHOLD 32
#define GROUP3_THRESHOLD 48
#define GROUP4_THRESHOLD 4144
#define MAX_NUMBER_OF_SEQUENCES 4144 //a.k.a m

#define TOTAL_GROUPS 4

// Codeword sizes
/*
#define GROUP1_CODE_SIZE 7
#define GROUP2_CODE_SIZE 7
#define GROUP3_CODE_SIZE 7
#define GROUP4_CODE_SIZE 15
*/

#endif
