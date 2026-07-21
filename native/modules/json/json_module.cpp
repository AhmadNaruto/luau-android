#include "json_module.h"
#include "lualib.h"
#include "yyjson.h"
#include <stdlib.h>
#include <string.h>

// Forward declarations
static void push_yyjson_val(lua_State *L, yyjson_val *val);
static yyjson_mut_val *lua_to_yyjson_val(lua_State *L, int idx, yyjson_mut_doc *doc);

static void push_yyjson_val(lua_State *L, yyjson_val *val) {
    switch (yyjson_get_type(val)) {
        case YYJSON_TYPE_NULL:
            lua_pushnil(L);
            break;
        case YYJSON_TYPE_BOOL:
            lua_pushboolean(L, yyjson_get_bool(val));
            break;
        case YYJSON_TYPE_NUM:
            if (yyjson_is_real(val)) {
                lua_pushnumber(L, yyjson_get_num(val));
            } else {
                lua_pushinteger(L, yyjson_get_int(val));
            }
            break;
        case YYJSON_TYPE_STR:
            lua_pushlstring(L, yyjson_get_str(val), yyjson_get_len(val));
            break;
        case YYJSON_TYPE_ARR: {
            lua_newtable(L);
            size_t idx, max;
            yyjson_val *item;
            yyjson_arr_foreach(val, idx, max, item) {
                push_yyjson_val(L, item);
                if (!lua_isnil(L, -1)) {
                    lua_rawseti(L, -2, idx + 1);
                } else {
                    lua_pop(L, 1);
                }
            }
            break;
        }
        case YYJSON_TYPE_OBJ: {
            lua_newtable(L);
            size_t idx, max;
            yyjson_val *key, *item;
            yyjson_obj_foreach(val, idx, max, key, item) {
                const char *key_str = yyjson_get_str(key);
                push_yyjson_val(L, item);
                if (!lua_isnil(L, -1)) {
                    lua_setfield(L, -2, key_str);
                } else {
                    lua_pop(L, 1);
                }
            }
            break;
        }
        default:
            lua_pushnil(L);
            break;
    }
}

static bool detect_table_type(lua_State *L, int tbl_idx, int *max_index) {
    tbl_idx = lua_absindex(L, tbl_idx);
    *max_index = 0;
    bool has_numeric = false;
    bool has_other = false;

    lua_pushnil(L);
    while (lua_next(L, tbl_idx) != 0) {
        // key is at -2, value at -1
        int key_type = lua_type(L, -2);
        if (key_type == LUA_TNUMBER) {
            double k = lua_tonumber(L, -2);
            if (k > 0 && k == (int)k) {
                has_numeric = true;
                if ((int)k > *max_index) {
                    *max_index = (int)k;
                }
            } else {
                has_other = true;
            }
        } else {
            has_other = true;
        }
        lua_pop(L, 1); // pop value
    }

    if (has_other) {
        return false; // has string keys or other types -> object
    }
    return true; // only positive integer keys (or empty) -> array
}

static yyjson_mut_val *lua_to_yyjson_val(lua_State *L, int idx, yyjson_mut_doc *doc) {
    idx = lua_absindex(L, idx);
    int type = lua_type(L, idx);
    switch (type) {
        case LUA_TBOOLEAN:
            return yyjson_mut_bool(doc, lua_toboolean(L, idx));
        case LUA_TNUMBER: {
            double num = lua_tonumber(L, idx);
            if (num == (int)num) {
                return yyjson_mut_int(doc, (int)num);
            } else {
                return yyjson_mut_real(doc, num);
            }
        }
        case LUA_TSTRING: {
            size_t len = 0;
            const char *str = lua_tolstring(L, idx, &len);
            return yyjson_mut_strn(doc, str, len);
        }
        case LUA_TTABLE: {
            int max_index = 0;
            if (detect_table_type(L, idx, &max_index)) {
                yyjson_mut_val *arr = yyjson_mut_arr(doc);
                for (int i = 1; i <= max_index; ++i) {
                    lua_rawgeti(L, idx, i);
                    yyjson_mut_val *val = lua_to_yyjson_val(L, -1, doc);
                    yyjson_mut_arr_add_val(arr, val);
                    lua_pop(L, 1);
                }
                return arr;
            } else {
                yyjson_mut_val *obj = yyjson_mut_obj(doc);
                lua_pushnil(L);
                while (lua_next(L, idx) != 0) {
                    // key at -2, value at -1
                    lua_pushvalue(L, -2); // duplicate key
                    size_t klen = 0;
                    const char *key_str = lua_tolstring(L, -1, &klen);
                    
                    yyjson_mut_val *val = lua_to_yyjson_val(L, -2, doc); // convert value
                    yyjson_mut_obj_add_val(doc, obj, key_str, val);
                    
                    lua_pop(L, 2); // pop duplicated key & value
                }
                return obj;
            }
        }
        case LUA_TNIL:
        default:
            return yyjson_mut_null(doc);
    }
}

static int json_parse(lua_State *L) {
    size_t len = 0;
    const char *json_str = luaL_checklstring(L, 1, &len);

    yyjson_read_err err;
    yyjson_doc *doc = yyjson_read_opts((char*)json_str, len, 0, NULL, &err);
    if (!doc) {
        luaL_error(L, "Failed to parse JSON string: %s (at position %d)", err.msg, (int)err.pos);
        return 0;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    push_yyjson_val(L, root);
    yyjson_doc_free(doc);
    return 1;
}

static int json_stringify(lua_State *L) {
    luaL_checkany(L, 1);

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) {
        luaL_error(L, "Failed to create mutable JSON doc");
        return 0;
    }

    yyjson_mut_val *root = lua_to_yyjson_val(L, 1, doc);
    yyjson_mut_doc_set_root(doc, root);

    size_t len = 0;
    char *json_str = yyjson_mut_write(doc, 0, &len);
    if (!json_str) {
        yyjson_mut_doc_free(doc);
        luaL_error(L, "Failed to stringify JSON");
        return 0;
    }

    lua_pushlstring(L, json_str, len);
    free(json_str);
    yyjson_mut_doc_free(doc);
    return 1;
}

int luaopen_json(lua_State *L) {
    lua_newtable(L);
    
    lua_pushcfunction(L, json_parse, "parse");
    lua_setfield(L, -2, "parse");
    
    lua_pushcfunction(L, json_stringify, "stringify");
    lua_setfield(L, -2, "stringify");

    return 1;
}
