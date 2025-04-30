#ifndef MEMORY_MANAGEMENT_H
#define MEMORY_MANAGEMENT_H

#include <stdlib.h>

#ifdef DEBUG_MEMORY

void* debug_malloc(size_t size, const char* file, int line, const char* type);
void* debug_calloc(size_t count, size_t size, const char* file, int line, const char* type);
void* debug_realloc(void* ptr, size_t new_size, const char* file, int line, const char* type);
void debug_free(void* ptr);
void dump_memory_leaks(void);
void reset_memory_tracker(void);

#define MEM_ALLOC(size, type)       debug_malloc(size, __FILE__, __LINE__, #type)
#define MEM_CALLOC(count, size, type) debug_calloc(count, size, __FILE__, __LINE__, #type)
#define MEM_REALLOC(ptr, size, type) debug_realloc(ptr, size, __FILE__, __LINE__, #type)
#define MEM_FREE(ptr)              debug_free(ptr)

#else

#define MEM_ALLOC(size, type)       malloc(size)
#define MEM_CALLOC(count, size, type) calloc(count, size)
#define MEM_REALLOC(ptr, size, type) realloc(ptr, size)
#define MEM_FREE(ptr)              free(ptr)

// No-op in release
inline static void dump_memory_leaks(void) {}
inline static void reset_memory_tracker(void) {}

#endif // DEBUG_MEMORY

#endif // MEMORY_MANAGEMENT_H
