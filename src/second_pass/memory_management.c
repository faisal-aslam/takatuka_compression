#ifdef DEBUG_MEMORY

#include "memory_management.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    void* ptr;
    size_t size;
    const char* file;
    int line;
    const char* type;
} AllocationRecord;

static AllocationRecord allocations[1024]; // Static memory
static int allocation_count = 0;

void* debug_malloc(size_t size, const char* file, int line, const char* type) {
    void* ptr = malloc(size);
    if (ptr) {
        allocations[allocation_count] = (AllocationRecord){
            .ptr = ptr,
            .size = size,
            .file = file,
            .line = line,
            .type = type
        };
        allocation_count++;
    }
    return ptr;
}

void debug_free(void* ptr) {
    if (!ptr) return;
    
    for (int i = 0; i < allocation_count; i++) {
        if (allocations[i].ptr == ptr) {
            // Remove from tracking by swapping with last element
            allocations[i] = allocations[allocation_count - 1];
            allocation_count--;
            free(ptr);
            return;
        }
    }
    
    // If we get here, the pointer wasn't tracked
    printf("WARNING: Freeing untracked pointer %p\n", ptr);
    free(ptr);
}

void dump_memory_leaks() {
    if (allocation_count == 0) {
        printf("No memory leaks detected!\n");
        return;
    }
    
    printf("\n=== MEMORY LEAK REPORT ===\n");
    printf("%d leaks detected:\n", allocation_count);
    
    for (int i = 0; i < allocation_count; i++) {
        AllocationRecord* r = &allocations[i];
        printf("- Leak %d: %zu bytes (%s) allocated at %s:%d\n",
               i+1, r->size, r->type, r->file, r->line);
    }
}

#endif // DEBUG_MEMORY
