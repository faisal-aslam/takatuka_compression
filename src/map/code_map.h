#ifndef CODE_MAP_H
#define CODE_MAP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    const uint8_t* seq;  // pointer to external block memory (not owned)
    uint8_t length;
    uint16_t code;
    uint8_t code_class;
    uint64_t hash;
    bool occupied;
} CodeMapEntry;

typedef struct {
    CodeMapEntry* entries;
    size_t capacity;
    size_t size;
} CodeMap;

void init_code_map(CodeMap* map, size_t capacity);
void free_code_map(CodeMap* map);
bool code_map_set(CodeMap* map, const uint8_t* seq, uint8_t len, uint16_t code, uint8_t code_class);
bool code_map_get(const CodeMap* map, const uint8_t* seq, uint8_t len, uint16_t* out_code, uint8_t* out_class);

#endif
