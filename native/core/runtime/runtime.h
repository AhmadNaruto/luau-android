#ifndef LUAU_RUNTIME_H
#define LUAU_RUNTIME_H

#include "lua.h"
#include "lualib.h"
#include "allocator.h"
#include <stdbool.h>

typedef struct {
    lua_State *L;
    luau_allocator_t allocator;
    double start_time;       // Execution start time (in seconds)
    double timeout_seconds;  // Max execution time (0 for no timeout)
    bool is_running;
} luau_runtime_t;

// Creates a new Luau runtime with a specified memory limit (in bytes, 0 for unlimited)
luau_runtime_t* luau_create_runtime(size_t memory_limit);

// Destroys the Luau runtime and releases all VM memory
void luau_destroy_runtime(luau_runtime_t *runtime);

// Executes a Luau script string. Enforces the execution timeout (in seconds, 0 for unlimited).
// If an error occurs, returns non-zero and sets *out_error to a heap-allocated error string.
int luau_execute_script(luau_runtime_t *runtime, const char *source, const char *chunkname, double timeout_seconds, char **out_error);

#endif // LUAU_RUNTIME_H
