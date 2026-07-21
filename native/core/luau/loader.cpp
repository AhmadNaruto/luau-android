#include "loader.h"
#include "lualib.h"
#include <string.h>

static int luau_require(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);

    // 1. Check if the module is already loaded (cache lookup)
    lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
    lua_getfield(L, -1, name);
    if (!lua_isnil(L, -1)) {
        lua_remove(L, -2); // Remove the _LOADED table, leaving module value
        return 1;
    }
    lua_pop(L, 1); // Pop the nil value

    // 2. Lookup the module open function in registry
    lua_getfield(L, LUA_REGISTRYINDEX, "_MODULES");
    lua_getfield(L, -1, name);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 2); // Pop the nil and _MODULES table
        luaL_error(L, "module '%s' not found", name);
        return 0;
    }

    // Call open_function(name)
    lua_pushstring(L, name);
    lua_call(L, 1, 1); // openf returns 1 value (the module table/object)

    // Stack is now: [ _LOADED, _MODULES, module_value ]
    // 3. Cache the loaded value in _LOADED
    lua_pushvalue(L, -3); // Duplicate _LOADED
    lua_pushvalue(L, -2); // Duplicate module_value
    lua_setfield(L, -2, name); // _LOADED[name] = module_value
    lua_pop(L, 1); // Pop duplicated _LOADED

    // Clean up stack, returning only the module value
    lua_remove(L, -2); // Remove _MODULES
    lua_remove(L, -2); // Remove _LOADED

    return 1;
}

static int custom_collectgarbage(lua_State *L) {
    lua_gc(L, LUA_GCCOLLECT, 0);
    return 0;
}

void luau_init_modules(lua_State *L) {
    // Create cache table _LOADED in registry
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, "_LOADED");

    // Create registry table _MODULES in registry
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, "_MODULES");

    // Set global require function
    lua_pushcfunction(L, luau_require, "require");
    lua_setglobal(L, "require");

    // Set global collectgarbage function for testing
    lua_pushcfunction(L, custom_collectgarbage, "collectgarbage");
    lua_setglobal(L, "collectgarbage");
}

void luau_register_module(lua_State *L, const char *name, lua_CFunction openf) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_MODULES");
    lua_pushcfunction(L, openf, name);
    lua_setfield(L, -2, name);
    lua_pop(L, 1); // Pop _MODULES
}
