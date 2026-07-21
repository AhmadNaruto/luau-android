#ifndef LUAU_ALLOCATOR_H
#define LUAU_ALLOCATOR_H

#include <stddef.h>

typedef struct {
    size_t memory_limit; // 0 for no limit
    size_t memory_used;
    size_t peak_memory_used;
} luau_allocator_t;

void* luau_alloc(void *ud, void *ptr, size_t osize, size_t nsize);

#endif // LUAU_ALLOCATOR_H
