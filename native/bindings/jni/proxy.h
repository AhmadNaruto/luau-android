#ifndef LUAU_JNI_PROXY_H
#define LUAU_JNI_PROXY_H

#include "lua.h"

// Registers the metatables and the 'jni' module
int luaopen_jni(lua_State *L);

#endif // LUAU_JNI_PROXY_H
