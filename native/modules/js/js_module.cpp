#include "js_module.h"
#include "lualib.h"
#include "quickjs.h"
#include <stdlib.h>
#include <string.h>

// ─── Userdata type name ───────────────────────────────────────────────────────

#define JSCTX_MT "LuauJSContext"

typedef struct {
    JSRuntime *rt;
    JSContext *ctx;
    size_t    memory_limit;
} js_ctx_t;

// ─── JSValue → Lua conversion ─────────────────────────────────────────────────

static void jsval_to_lua(lua_State *L, JSContext *ctx, JSValue val) {
    if (JS_IsBool(val)) {
        lua_pushboolean(L, JS_ToBool(ctx, val));
    } else if (JS_IsNull(val) || JS_IsUndefined(val)) {
        lua_pushnil(L);
    } else if (JS_IsNumber(val)) {
        double d = 0.0;
        JS_ToFloat64(ctx, &d, val);
        lua_pushnumber(L, d);
    } else if (JS_IsString(val)) {
        size_t len = 0;
        const char *str = JS_ToCStringLen2(ctx, &len, val, false);
        if (str) {
            lua_pushlstring(L, str, len);
            JS_FreeCString(ctx, str);
        } else {
            lua_pushstring(L, "");
        }
    } else if (JS_IsObject(val)) {
        // Serialize objects to JSON string via JSON.stringify
        JSValue global = JS_GetGlobalObject(ctx);
        JSValue json   = JS_GetPropertyStr(ctx, global, "JSON");
        JSValue strfn  = JS_GetPropertyStr(ctx, json, "stringify");
        JSValue result = JS_Call(ctx, strfn, global, 1, &val);
        JS_FreeValue(ctx, strfn);
        JS_FreeValue(ctx, json);
        JS_FreeValue(ctx, global);
        if (!JS_IsException(result)) {
            size_t len = 0;
            const char *str = JS_ToCStringLen2(ctx, &len, result, false);
            if (str) { lua_pushlstring(L, str, len); JS_FreeCString(ctx, str); }
            else      { lua_pushstring(L, "{}"); }
            JS_FreeValue(ctx, result);
        } else {
            JS_FreeValue(ctx, result);
            lua_pushstring(L, "[object]");
        }
    } else {
        lua_pushstring(L, "[unknown]");
    }
}

// ─── Lua value → JSValue conversion ──────────────────────────────────────────

static JSValue luaval_to_js(lua_State *L, JSContext *ctx, int idx) {
    int t = lua_type(L, idx);
    switch (t) {
        case LUA_TNIL:     return JS_NULL;
        case LUA_TBOOLEAN: return JS_NewBool(ctx, lua_toboolean(L, idx));
        case LUA_TNUMBER:  return JS_NewFloat64(ctx, lua_tonumber(L, idx));
        case LUA_TSTRING: {
            size_t len;
            const char *s = lua_tolstring(L, idx, &len);
            return JS_NewStringLen(ctx, s, len);
        }
        default:
            return JS_UNDEFINED;
    }
}

// ─── Error helper ─────────────────────────────────────────────────────────────

static int push_js_exception(lua_State *L, JSContext *ctx) {
    JSValue exc = JS_GetException(ctx);
    if (JS_IsNull(exc) || JS_IsUndefined(exc)) {
        luaL_error(L, "js: unknown JavaScript error");
    } else {
        // Try to get message property first
        JSValue msg = JS_GetPropertyStr(ctx, exc, "message");
        if (!JS_IsUndefined(msg) && !JS_IsException(msg)) {
            const char *s = JS_ToCString(ctx, msg);
            if (s) {
                JS_FreeValue(ctx, msg);
                JS_FreeValue(ctx, exc);
                luaL_error(L, "js error: %s", s);
            }
            JS_FreeCString(ctx, s);
            JS_FreeValue(ctx, msg);
        } else {
            JS_FreeValue(ctx, msg);
            const char *s = JS_ToCString(ctx, exc);
            if (s) {
                JS_FreeValue(ctx, exc);
                luaL_error(L, "js error: %s", s);
            }
            JS_FreeCString(ctx, s);
        }
        JS_FreeValue(ctx, exc);
    }
    luaL_error(L, "js: unknown error");
    return 0;
}

// ─── ctx:eval(code) → Lua value ───────────────────────────────────────────────

static int js_ctx_eval(lua_State *L) {
    js_ctx_t *jc = (js_ctx_t*)luaL_checkudata(L, 1, JSCTX_MT);
    size_t code_len;
    const char *code = luaL_checklstring(L, 2, &code_len);

    JSValue result = JS_Eval(jc->ctx, code, code_len, "<eval>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        JS_FreeValue(jc->ctx, result);
        return push_js_exception(L, jc->ctx);
    }

    jsval_to_lua(L, jc->ctx, result);
    JS_FreeValue(jc->ctx, result);
    return 1;
}

// ─── ctx:set(name, value) ────────────────────────────────────────────────────

static int js_ctx_set(lua_State *L) {
    js_ctx_t *jc = (js_ctx_t*)luaL_checkudata(L, 1, JSCTX_MT);
    const char *name = luaL_checkstring(L, 2);

    JSValue jsval = luaval_to_js(L, jc->ctx, 3);
    JSValue global = JS_GetGlobalObject(jc->ctx);
    JS_SetPropertyStr(jc->ctx, global, name, jsval); // jsval ownership transferred
    JS_FreeValue(jc->ctx, global);
    return 0;
}

// ─── ctx:get(name) → Lua value ────────────────────────────────────────────────

static int js_ctx_get(lua_State *L) {
    js_ctx_t *jc = (js_ctx_t*)luaL_checkudata(L, 1, JSCTX_MT);
    const char *name = luaL_checkstring(L, 2);

    JSValue global = JS_GetGlobalObject(jc->ctx);
    JSValue val    = JS_GetPropertyStr(jc->ctx, global, name);
    JS_FreeValue(jc->ctx, global);

    if (JS_IsException(val)) {
        JS_FreeValue(jc->ctx, val);
        return push_js_exception(L, jc->ctx);
    }

    jsval_to_lua(L, jc->ctx, val);
    JS_FreeValue(jc->ctx, val);
    return 1;
}

// ─── ctx:gc() ────────────────────────────────────────────────────────────────

static int js_ctx_gc(lua_State *L) {
    js_ctx_t *jc = (js_ctx_t*)luaL_checkudata(L, 1, JSCTX_MT);
    JS_RunGC(jc->rt);
    return 0;
}

// ─── __gc metamethod ─────────────────────────────────────────────────────────

static int js_ctx_gc_mm(lua_State *L) {
    js_ctx_t *jc = (js_ctx_t*)luaL_checkudata(L, 1, JSCTX_MT);
    if (jc->ctx) { JS_FreeContext(jc->ctx); jc->ctx = NULL; }
    if (jc->rt)  { JS_FreeRuntime(jc->rt);  jc->rt  = NULL; }
    return 0;
}

// ─── js.new([memory_limit_bytes]) ────────────────────────────────────────────

static int js_new(lua_State *L) {
    size_t mem_limit = (size_t)luaL_optinteger(L, 1, 0);

    js_ctx_t *jc = (js_ctx_t*)lua_newuserdata(L, sizeof(js_ctx_t));
    jc->rt  = NULL;
    jc->ctx = NULL;
    jc->memory_limit = mem_limit;

    luaL_getmetatable(L, JSCTX_MT);
    lua_setmetatable(L, -2);

    jc->rt = JS_NewRuntime();
    if (!jc->rt) { luaL_error(L, "js.new: failed to create JS runtime"); return 0; }

    if (mem_limit > 0) JS_SetMemoryLimit(jc->rt, mem_limit);

    jc->ctx = JS_NewContext(jc->rt);
    if (!jc->ctx) {
        JS_FreeRuntime(jc->rt); jc->rt = NULL;
        luaL_error(L, "js.new: failed to create JS context");
        return 0;
    }

    return 1;
}

// ─── Module open ─────────────────────────────────────────────────────────────

int luaopen_js(lua_State *L) {
    // Register JSContext metatable
    luaL_newmetatable(L, JSCTX_MT);

    lua_pushstring(L, "__index");
    lua_newtable(L);
    lua_pushcfunction(L, js_ctx_eval, "eval"); lua_setfield(L, -2, "eval");
    lua_pushcfunction(L, js_ctx_set,  "set");  lua_setfield(L, -2, "set");
    lua_pushcfunction(L, js_ctx_get,  "get");  lua_setfield(L, -2, "get");
    lua_pushcfunction(L, js_ctx_gc,   "gc");   lua_setfield(L, -2, "gc");
    lua_settable(L, -3); // metatable.__index = methods table

    lua_pushstring(L, "__gc");
    lua_pushcfunction(L, js_ctx_gc_mm, "__gc");
    lua_settable(L, -3);
    lua_pop(L, 1); // pop metatable

    // Module table
    lua_newtable(L);
    lua_pushcfunction(L, js_new, "new");
    lua_setfield(L, -2, "new");

    return 1;
}
