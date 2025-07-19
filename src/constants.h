#pragma once

#define TOTAL_GRAPH_NODES(SEQ_LIMIT, LEVELS) \
    (1 + ((SEQ_LIMIT) * ((SEQ_LIMIT) + 1)) / 2 + ((LEVELS) - (SEQ_LIMIT)) * (SEQ_LIMIT))

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

#define MAX_GRAPH_NODES TOTAL_GRAPH_NODES(SEQ_LENGTH_LIMIT, BLOCK_SIZE)

#define TOTAL_GROUPS 4

