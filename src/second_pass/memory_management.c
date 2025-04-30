#ifdef DEBUG_MEMORY
/**
Replace all malloc and free calls in your code with: 
                  MEM_ALLOC(size, type) instead of malloc(size)
                  MEM_FREE(ptr) instead of free(ptr)
And similarly:
MEM_CALLOC(count, size, type) instead of calloc(count, size)
MEM_REALLOC(ptr, new_size, type) instead of realloc(ptr, new_size)

*/
#include "memory_management.h"
#include <stdio.h>
#include <string.h>

#define MAX_TRACKED_ALLOCATIONS 1024

typedef struct {
    void* ptr;
    size_t size;
    const char* file;
    int line;
    const char* type;
} AllocationRecord;

static AllocationRecord allocations[MAX_TRACKED_ALLOCATIONS]; // Static memory
static int allocation_count = 0;
static size_t total_allocated = 0;

static int find_allocation_index(void* ptr) {
    for (int i = 0; i < allocation_count; ++i) {
        if (allocations[i].ptr == ptr)
            return i;
    }
    return -1;
}

void* debug_malloc(size_t size, const char* file, int line, const char* type) {
    if (allocation_count >= MAX_TRACKED_ALLOCATIONS) {
        fprintf(stderr, "ERROR: Memory tracking limit exceeded at %s:%d for type %s\n", file, line, type);
        return NULL;
    }

    void* ptr = malloc(size);
    if (ptr) {
        allocations[allocation_count++] = (AllocationRecord){
            .ptr = ptr,
            .size = size,
            .file = file,
            .line = line,
            .type = type
        };
        total_allocated += size;
    }
    return ptr;
}

void* debug_calloc(size_t count, size_t size, const char* file, int line, const char* type) {
    size_t total_size = count * size;
    if (allocation_count >= MAX_TRACKED_ALLOCATIONS) {
        fprintf(stderr, "ERROR: Memory tracking limit exceeded at %s:%d for type %s\n", file, line, type);
        return NULL;
    }

    void* ptr = calloc(count, size);
    if (ptr) {
        allocations[allocation_count++] = (AllocationRecord){
            .ptr = ptr,
            .size = total_size,
            .file = file,
            .line = line,
            .type = type
        };
        total_allocated += total_size;
    }
    return ptr;
}

void* debug_realloc(void* ptr, size_t new_size, const char* file, int line, const char* type) {
    if (!ptr) {
        // realloc behaves like malloc
        return debug_malloc(new_size, file, line, type);
    }

    int idx = find_allocation_index(ptr);
    if (idx == -1) {
        fprintf(stderr, "WARNING: Attempted to realloc untracked pointer %p at %s:%d\n", ptr, file, line);
        return realloc(ptr, new_size); // fall back
    }

    void* new_ptr = realloc(ptr, new_size);
    if (new_ptr) {
        total_allocated -= allocations[idx].size;
        total_allocated += new_size;

        allocations[idx] = (AllocationRecord){
            .ptr = new_ptr,
            .size = new_size,
            .file = file,
            .line = line,
            .type = type
        };
    }

    return new_ptr;
}

void debug_free(void* ptr) {
    if (!ptr) return;

    int idx = find_allocation_index(ptr);
    if (idx != -1) {
        total_allocated -= allocations[idx].size;
        allocations[idx] = allocations[--allocation_count]; // swap and shrink
        free(ptr);
    } else {
        fprintf(stderr, "WARNING: Attempted to free untracked pointer %p\n", ptr);
        free(ptr);
    }
}

void dump_memory_leaks(void) {
    if (allocation_count == 0) {
        printf("No memory leaks detected.\n");
        return;
    }

    printf("\n=== MEMORY LEAK REPORT ===\n");
    printf("Total leaks: %d\n", allocation_count);
    printf("Total bytes still allocated: %zu\n", total_allocated);

    for (int i = 0; i < allocation_count; ++i) {
        AllocationRecord* r = &allocations[i];
        printf("- Leak %d: %zu bytes (%s) allocated at %s:%d\n",
               i + 1, r->size, r->type, r->file, r->line);
    }
}

void reset_memory_tracker(void) {
    allocation_count = 0;
    total_allocated = 0;
}

#endif // DEBUG_MEMORY
