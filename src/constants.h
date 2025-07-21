#pragma once

#define TOTAL_GRAPH_NODES(SEQ_LIMIT, LEVELS) \
    (1 /* root */ + \
    (((SEQ_LIMIT) * ((SEQ_LIMIT) + 1)) / 2 + (SEQ_LIMIT)) /* levels 1 to SEQ_LIMIT */ + \
    ((LEVELS) > (SEQ_LIMIT) ? ((LEVELS) - (SEQ_LIMIT)) * ((SEQ_LIMIT) + 1) : 0) /* levels > SEQ_LIMIT */ )


#include <stdlib.h>
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define SEQ_LENGTH_START 2
#define SEQ_LENGTH_LIMIT 128 //a.k.a k
#define BLOCK_SIZE 64000

_Static_assert(SEQ_LENGTH_LIMIT <= 254, "SEQ_LENGTH_LIMIT must be ≤ 254");
_Static_assert(BLOCK_SIZE <= 65535, "BLOCK_SIZE must be ≤ 65,535");

#define MAX_GRAPH_NODES TOTAL_GRAPH_NODES(SEQ_LENGTH_LIMIT, BLOCK_SIZE)

#define TOTAL_GROUPS 4

