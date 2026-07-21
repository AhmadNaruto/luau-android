#include "runtime.h"
#include "allocator.h"
#include "luacode.h"
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

// Module system headers
#include "loader.h"
#include "html_module.h"
#include "js_module.h"
#include "json_module.h"
#include "regex_module.h"
#include "url_module.h"
#include "encoding_module.h"
#include "crypto_module.h"
#include "buffer_module.h"
#include "util_module.h"
#include "time_module.h"
#include "select_module.h"
#include "proxy.h"

static double get_time_seconds(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

static void luau_interrupt_callback(lua_State *L, int gc) {
    luau_runtime_t *runtime = (luau_runtime_t*)lua_getthreaddata(L);
    if (runtime && runtime->timeout_seconds > 0 && runtime->is_running) {
        double current_time = get_time_seconds();
        if (current_time - runtime->start_time > runtime->timeout_seconds) {
            luaL_error(L, "Script execution timeout");
        }
    }
}

luau_runtime_t* luau_create_runtime(size_t memory_limit) {
    luau_runtime_t *runtime = (luau_runtime_t*)malloc(sizeof(luau_runtime_t));
    if (!runtime) return NULL;

    runtime->allocator.memory_limit = memory_limit;
    runtime->allocator.memory_used = 0;
    runtime->allocator.peak_memory_used = 0;
    runtime->start_time = 0.0;
    runtime->timeout_seconds = 0.0;
    runtime->is_running = false;
    runtime->env = NULL;

    // Create lua_State with our custom allocator
    runtime->L = lua_newstate(luau_alloc, &runtime->allocator);
    if (!runtime->L) {
        free(runtime);
        return NULL;
    }

    // Set thread data so we can retrieve the runtime in callbacks
    lua_setthreaddata(runtime->L, runtime);

    // Open standard libraries
    luaL_openlibs(runtime->L);

    // Set interrupt callback
    lua_callbacks(runtime->L)->interrupt = luau_interrupt_callback;

    // Initialize modules loader and register native modules
    luau_init_modules(runtime->L);
    luau_register_module(runtime->L, "html", luaopen_html);
    luau_register_module(runtime->L, "js", luaopen_js);
    luau_register_module(runtime->L, "json", luaopen_json);
    luau_register_module(runtime->L, "regex", luaopen_regex);
    luau_register_module(runtime->L, "url", luaopen_url);
    luau_register_module(runtime->L, "encoding", luaopen_encoding);
    luau_register_module(runtime->L, "crypto", luaopen_crypto);
    luau_register_module(runtime->L, "buffer", luaopen_custom_buffer);
    luau_register_module(runtime->L, "util", luaopen_util);
    luau_register_module(runtime->L, "time", luaopen_time);
    luau_register_module(runtime->L, "select", luaopen_select);
    luau_register_module(runtime->L, "jni", luaopen_jni);

    return runtime;
}

void luau_destroy_runtime(luau_runtime_t *runtime) {
    if (!runtime) return;

    if (runtime->L) {
        lua_close(runtime->L);
    }
    free(runtime);
}

int luau_execute_script(luau_runtime_t *runtime, const char *source, const char *chunkname, double timeout_seconds, char **out_error) {
    if (!runtime || !source) {
        if (out_error) *out_error = strdup("Invalid arguments");
        return -1;
    }

    // Prepare compile options
    lua_CompileOptions compile_options;
    memset(&compile_options, 0, sizeof(compile_options));
    compile_options.optimizationLevel = 1;
    compile_options.debugLevel = 1;

    size_t bytecode_size = 0;
    char *bytecode = luau_compile(source, strlen(source), &compile_options, &bytecode_size);
    if (!bytecode) {
        if (out_error) *out_error = strdup("Compilation failed (no bytecode produced)");
        return -1;
    }

    // Load compiled bytecode
    int status = luau_load(runtime->L, chunkname ? chunkname : "=script", bytecode, bytecode_size, 0);
    free(bytecode);

    if (status != 0) {
        if (out_error) {
            const char *err = lua_tostring(runtime->L, -1);
            *out_error = strdup(err ? err : "Syntax error");
        }
        lua_pop(runtime->L, 1); // Remove error from stack
        return status;
    }

    // Setup execution tracking
    runtime->timeout_seconds = timeout_seconds;
    runtime->start_time = get_time_seconds();
    runtime->is_running = true;

    // Run the chunk
    status = lua_pcall(runtime->L, 0, LUA_MULTRET, 0);

    runtime->is_running = false;

    if (status != 0) {
        if (out_error) {
            const char *err = lua_tostring(runtime->L, -1);
            *out_error = strdup(err ? err : "Runtime error");
        }
        lua_pop(runtime->L, 1); // Remove error from stack
    }

    return status;
}
