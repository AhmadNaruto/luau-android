#include "util_module.h"
#include "lualib.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <random>
#include <stdio.h>

// util.trim(str) -> str
static int util_trim(lua_State *L) {
    size_t len = 0;
    const char *str = luaL_checklstring(L, 1, &len);
    size_t start = 0;
    while (start < len && isspace((unsigned char)str[start])) start++;
    size_t end = len;
    while (end > start && isspace((unsigned char)str[end - 1])) end--;
    lua_pushlstring(L, str + start, end - start);
    return 1;
}

// util.split(str, sep) -> table
static int util_split(lua_State *L) {
    size_t slen = 0, seplen = 0;
    const char *str = luaL_checklstring(L, 1, &slen);
    const char *sep = luaL_checklstring(L, 2, &seplen);

    lua_newtable(L);
    if (seplen == 0) {
        lua_pushlstring(L, str, slen);
        lua_rawseti(L, -2, 1);
        return 1;
    }

    int count = 0;
    const char *p = str;
    const char *end = str + slen;
    while (p <= end) {
        const char *found = strstr(p, sep);
        if (!found) {
            lua_pushlstring(L, p, end - p);
            lua_rawseti(L, -2, ++count);
            break;
        }
        lua_pushlstring(L, p, found - p);
        lua_rawseti(L, -2, ++count);
        p = found + seplen;
    }
    return 1;
}

// util.join(tbl, sep) -> str
static int util_join(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    const char *sep = luaL_optstring(L, 2, "");
    size_t seplen = strlen(sep);

    size_t len = lua_objlen(L, 1);
    if (len == 0) {
        lua_pushstring(L, "");
        return 1;
    }

    luaL_Strbuf b;
    luaL_buffinit(L, &b);

    for (size_t i = 1; i <= len; i++) {
        lua_rawgeti(L, 1, i);
        if (i > 1 && seplen > 0) {
            luaL_addlstring(&b, sep, seplen);
        }
        size_t elen = 0;
        const char *s = luaL_tolstring(L, -1, &elen);
        luaL_addlstring(&b, s, elen);
        lua_pop(L, 2); // pop element & tolstring
    }

    luaL_pushresult(&b);
    return 1;
}

// util.startswith(str, prefix) -> bool
static int util_startswith(lua_State *L) {
    size_t slen = 0, plen = 0;
    const char *str = luaL_checklstring(L, 1, &slen);
    const char *prefix = luaL_checklstring(L, 2, &plen);
    if (plen > slen) {
        lua_pushboolean(L, 0);
    } else {
        lua_pushboolean(L, memcmp(str, prefix, plen) == 0);
    }
    return 1;
}

// util.endswith(str, suffix) -> bool
static int util_endswith(lua_State *L) {
    size_t slen = 0, suflen = 0;
    const char *str = luaL_checklstring(L, 1, &slen);
    const char *suffix = luaL_checklstring(L, 2, &suflen);
    if (suflen > slen) {
        lua_pushboolean(L, 0);
    } else {
        lua_pushboolean(L, memcmp(str + slen - suflen, suffix, suflen) == 0);
    }
    return 1;
}

// util.contains(str, substr) -> bool
static int util_contains(lua_State *L) {
    size_t slen = 0, sublen = 0;
    const char *str = luaL_checklstring(L, 1, &slen);
    const char *sub = luaL_checklstring(L, 2, &sublen);
    if (sublen == 0) {
        lua_pushboolean(L, 1);
    } else if (sublen > slen) {
        lua_pushboolean(L, 0);
    } else {
        lua_pushboolean(L, strstr(str, sub) != NULL);
    }
    return 1;
}

// util.lower(str) -> str
static int util_lower(lua_State *L) {
    size_t len = 0;
    const char *str = luaL_checklstring(L, 1, &len);
    char *buf = (char*)malloc(len + 1);
    for (size_t i = 0; i < len; i++) {
        buf[i] = (char)tolower((unsigned char)str[i]);
    }
    lua_pushlstring(L, buf, len);
    free(buf);
    return 1;
}

// util.upper(str) -> str
static int util_upper(lua_State *L) {
    size_t len = 0;
    const char *str = luaL_checklstring(L, 1, &len);
    char *buf = (char*)malloc(len + 1);
    for (size_t i = 0; i < len; i++) {
        buf[i] = (char)toupper((unsigned char)str[i]);
    }
    lua_pushlstring(L, buf, len);
    free(buf);
    return 1;
}

// util.capitalize(str) -> str
static int util_capitalize(lua_State *L) {
    size_t len = 0;
    const char *str = luaL_checklstring(L, 1, &len);
    if (len == 0) {
        lua_pushstring(L, "");
        return 1;
    }
    char *buf = (char*)malloc(len + 1);
    memcpy(buf, str, len);
    buf[0] = (char)toupper((unsigned char)buf[0]);
    for (size_t i = 1; i < len; i++) {
        buf[i] = (char)tolower((unsigned char)buf[i]);
    }
    lua_pushlstring(L, buf, len);
    free(buf);
    return 1;
}

// util.sleep(ms)
static int util_sleep(lua_State *L) {
    double ms = luaL_checknumber(L, 1);
    usleep((useconds_t)(ms * 1000.0));
    return 0;
}

// util.uuid() -> str
static int util_uuid(lua_State *L) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);

    uint32_t data[4];
    for (int i = 0; i < 4; i++) data[i] = dis(gen);

    // Set UUID v4 variant & version
    data[1] = (data[1] & 0xFFFF0FFF) | 0x00004000;
    data[2] = (data[2] & 0x3FFFFFFF) | 0x80000000;

    char uuid_str[37];
    snprintf(uuid_str, sizeof(uuid_str),
             "%08x-%04x-%04x-%04x-%04x%08x",
             data[0],
             data[1] >> 16, data[1] & 0xFFFF,
             data[2] >> 16, data[2] & 0xFFFF,
             data[3]);

    lua_pushstring(L, uuid_str);
    return 1;
}

int luaopen_util(lua_State *L) {
    lua_newtable(L);

    lua_pushcfunction(L, util_trim,       "trim");       lua_setfield(L, -2, "trim");
    lua_pushcfunction(L, util_split,      "split");      lua_setfield(L, -2, "split");
    lua_pushcfunction(L, util_join,       "join");       lua_setfield(L, -2, "join");
    lua_pushcfunction(L, util_startswith, "startswith"); lua_setfield(L, -2, "startswith");
    lua_pushcfunction(L, util_endswith,   "endswith");   lua_setfield(L, -2, "endswith");
    lua_pushcfunction(L, util_contains,   "contains");   lua_setfield(L, -2, "contains");
    lua_pushcfunction(L, util_lower,      "lower");      lua_setfield(L, -2, "lower");
    lua_pushcfunction(L, util_upper,      "upper");      lua_setfield(L, -2, "upper");
    lua_pushcfunction(L, util_capitalize, "capitalize"); lua_setfield(L, -2, "capitalize");
    lua_pushcfunction(L, util_sleep,      "sleep");      lua_setfield(L, -2, "sleep");
    lua_pushcfunction(L, util_uuid,       "uuid");       lua_setfield(L, -2, "uuid");

    return 1;
}
