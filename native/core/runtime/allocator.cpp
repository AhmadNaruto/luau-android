#include "allocator.h"
#include <stdlib.h>

void* luau_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    luau_allocator_t *alloc = (luau_allocator_t*)ud;

    if (nsize == 0) {
        if (ptr) {
            free(ptr);
            if (alloc && alloc->memory_used >= osize) {
                alloc->memory_used -= osize;
            }
        }
        return NULL;
    }

    if (alloc && alloc->memory_limit > 0) {
        size_t needed = nsize;
        size_t current_osize = ptr ? osize : 0;
        if (alloc->memory_used + needed - current_osize > alloc->memory_limit) {
            return NULL; // Exceeds memory limit
        }
    }

    void *new_ptr = realloc(ptr, nsize);
    if (new_ptr) {
        if (alloc) {
            size_t current_osize = ptr ? osize : 0;
            alloc->memory_used = alloc->memory_used + nsize - current_osize;
            if (alloc->memory_used > alloc->peak_memory_used) {
                alloc->peak_memory_used = alloc->memory_used;
            }
        }
    }
    return new_ptr;
}
