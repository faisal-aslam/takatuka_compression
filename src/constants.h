#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <stdlib.h>
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define SEQ_LENGTH_START 2
#define SEQ_LENGTH_LIMIT 16 //a.k.a k
#define BLOCK_SIZE 2000

#define TOTAL_GROUPS 4

#endif
