#ifndef LUAU_LOADER_H
#define LUAU_LOADER_H

#include "lua.h"

// Initializes the module loader system for the given state (creates registry and cache tables)
void luau_init_modules(lua_State *L);

// Registers a native module with a name and its open function
void luau_register_module(lua_State *L, const char *name, lua_CFunction openf);

#endif // LUAU_LOADER_H
