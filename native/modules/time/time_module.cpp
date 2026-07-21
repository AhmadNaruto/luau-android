#include "time_module.h"
#include "lualib.h"
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>

// time.now() -> number (epoch time in seconds as float/double with subsecond precision)
static int time_now(lua_State *L) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double sec = (double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0);
    lua_pushnumber(L, sec);
    return 1;
}

// time.unix() -> number (integer unix timestamp in seconds)
static int time_unix(lua_State *L) {
    time_t t = time(NULL);
    lua_pushinteger(L, (int)t);
    return 1;
}

// time.format(timestamp, fmt_str) -> string
static int time_format(lua_State *L) {
    double ts = luaL_optnumber(L, 1, (double)time(NULL));
    const char *fmt = luaL_optstring(L, 2, "%Y-%m-%d %H:%M:%S");

    time_t t = (time_t)ts;
    struct tm tm_info;
    localtime_r(&t, &tm_info);

    char buf[128];
    size_t len = strftime(buf, sizeof(buf), fmt, &tm_info);
    lua_pushlstring(L, buf, len);
    return 1;
}

// time.parse(str, fmt_str) -> number (epoch timestamp)
static int time_parse(lua_State *L) {
    const char *str = luaL_checkstring(L, 1);
    const char *fmt = luaL_optstring(L, 2, "%Y-%m-%d %H:%M:%S");

    struct tm tm_info;
    memset(&tm_info, 0, sizeof(tm_info));

    char *res = strptime(str, fmt, &tm_info);
    if (!res) {
        lua_pushnil(L);
        return 1;
    }

    time_t t = mktime(&tm_info);
    lua_pushinteger(L, (int)t);
    return 1;
}

// time.sleep(ms)
static int time_sleep(lua_State *L) {
    double ms = luaL_checknumber(L, 1);
    usleep((useconds_t)(ms * 1000.0));
    return 0;
}

int luaopen_time(lua_State *L) {
    lua_newtable(L);

    lua_pushcfunction(L, time_now,    "now");    lua_setfield(L, -2, "now");
    lua_pushcfunction(L, time_unix,   "unix");   lua_setfield(L, -2, "unix");
    lua_pushcfunction(L, time_format, "format"); lua_setfield(L, -2, "format");
    lua_pushcfunction(L, time_parse,  "parse");  lua_setfield(L, -2, "parse");
    lua_pushcfunction(L, time_sleep,  "sleep");  lua_setfield(L, -2, "sleep");

    return 1;
}
