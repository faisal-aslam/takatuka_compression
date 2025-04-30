#ifndef MEMORY_MANAGEMENT_H
#define MEMORY_MANAGEMENT_H

#include <stdlib.h>

#ifdef DEBUG_MEMORY

void* debug_malloc(size_t size, const char* file, int line, const char* type);
void debug_free(void* ptr);
void dump_memory_leaks();

#define MEM_ALLOC(size, type) debug_malloc(size, __FILE__, __LINE__, #type)
#define MEM_FREE(ptr) debug_free(ptr)

#else

#define MEM_ALLOC(size, type) malloc(size)
#define MEM_FREE(ptr) free(ptr)
#define dump_memory_leaks()

#endif

#endif // MEMORY_MANAGEMENT_H
