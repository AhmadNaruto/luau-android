#define PCRE2_CODE_UNIT_WIDTH 8
#include "regex_module.h"
#include "lualib.h"
#include "pcre2.h"
#include <stdlib.h>
#include <string.h>

// Userdata layout
typedef struct {
    pcre2_code *code;
} regex_t;

static int regex_compile(lua_State *L) {
    const char *pattern = luaL_checkstring(L, 1);

    int errornumber;
    PCRE2_SIZE erroroffset;
    pcre2_code *code = pcre2_compile(
        (PCRE2_SPTR)pattern,
        PCRE2_ZERO_TERMINATED,
        0, // options
        &errornumber,
        &erroroffset,
        NULL
    );

    if (!code) {
        PCRE2_UCHAR error_buf[256];
        pcre2_get_error_message(errornumber, error_buf, sizeof(error_buf));
        luaL_error(L, "Failed to compile regex pattern: %s (at offset %d)", (char*)error_buf, (int)erroroffset);
        return 0;
    }

    regex_t *re = (regex_t*)lua_newuserdata(L, sizeof(regex_t));
    re->code = code;

    luaL_getmetatable(L, "LuauRegex");
    lua_setmetatable(L, -2);
    return 1;
}

static int regex_gc(lua_State *L) {
    regex_t *re = (regex_t*)lua_touserdata(L, 1);
    if (re && re->code) {
        pcre2_code_free(re->code);
        re->code = NULL;
    }
    return 0;
}

static int regex_match(lua_State *L) {
    regex_t *re = (regex_t*)luaL_checkudata(L, 1, "LuauRegex");
    size_t subject_len = 0;
    const char *subject = luaL_checklstring(L, 2, &subject_len);

    pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(re->code, NULL);
    if (!match_data) {
        luaL_error(L, "Failed to create PCRE2 match data");
        return 0;
    }

    int rc = pcre2_match(
        re->code,
        (PCRE2_SPTR)subject,
        subject_len,
        0, // start offset
        0, // options
        match_data,
        NULL
    );

    if (rc < 0) {
        pcre2_match_data_free(match_data);
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);

    // 1. Positional captures (1-based, where 0 is full match, 1 is first capture group)
    for (int i = 0; i < rc; i++) {
        PCRE2_SIZE start = ovector[2 * i];
        PCRE2_SIZE end = ovector[2 * i + 1];
        if (start != PCRE2_UNSET && end != PCRE2_UNSET) {
            lua_pushlstring(L, subject + start, end - start);
            lua_rawseti(L, -2, i);
        }
    }

    // 2. Named captures
    uint32_t name_count = 0;
    pcre2_pattern_info(re->code, PCRE2_INFO_NAMECOUNT, &name_count);
    if (name_count > 0) {
        PCRE2_SPTR name_table;
        uint32_t name_entry_size = 0;
        pcre2_pattern_info(re->code, PCRE2_INFO_NAMETABLE, &name_table);
        pcre2_pattern_info(re->code, PCRE2_INFO_NAMEENTRYSIZE, &name_entry_size);

        PCRE2_SPTR tabptr = name_table;
        for (uint32_t i = 0; i < name_count; i++) {
            uint32_t group_index = (tabptr[0] << 8) | tabptr[1];
            const char *name = (const char*)(tabptr + 2);

            if (group_index < (uint32_t)rc) {
                PCRE2_SIZE start = ovector[2 * group_index];
                PCRE2_SIZE end = ovector[2 * group_index + 1];
                if (start != PCRE2_UNSET && end != PCRE2_UNSET) {
                    lua_pushlstring(L, subject + start, end - start);
                    lua_setfield(L, -2, name);
                }
            }
            tabptr += name_entry_size;
        }
    }

    pcre2_match_data_free(match_data);
    return 1;
}

int luaopen_regex(lua_State *L) {
    // 1. Create metatable for LuauRegex
    luaL_newmetatable(L, "LuauRegex");
    lua_pushcfunction(L, regex_gc, "__gc");
    lua_setfield(L, -2, "__gc");

    lua_newtable(L);
    lua_pushcfunction(L, regex_match, "match");
    lua_setfield(L, -2, "match");
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    // 2. Create module functions
    lua_newtable(L);
    lua_pushcfunction(L, regex_compile, "compile");
    lua_setfield(L, -2, "compile");

    return 1;
}
