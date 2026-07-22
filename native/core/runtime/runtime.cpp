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

    // Initialize global compatibility helpers for novelx-sources extensions
    static const char *COMPATIBILITY_SHIM_SCRIPT = R"(
        local html   = require("html")
        local regex  = require("regex")
        local url    = require("url")
        local util   = require("util")

        -- String helpers
        string_trim        = util.trim
        string_starts_with = util.startswith
        string_ends_with   = util.endswith
        string_contains    = util.contains
        string_lower       = util.lower
        string_upper       = util.upper
        string_capitalize  = util.capitalize

        function string_normalize(str)
            if not str then return "" end
            return (str:gsub("%s+", " "))
        end

        function string_clean(str)
            if not str then return "" end
            return util.trim(string_normalize(str))
        end

        -- URL helpers
        url_resolve = function(base, href)
            if not href or href == "" then return "" end
            if not base or base == "" then return href end
            if href:find("^https?://") or href:find("^http://") then return href end
            if href:find("^//") then return "https:" .. href end
            local ok, res = pcall(url.join, base, href)
            if ok and res and #res > 0 then return res end
            return href
        end
        url_encode = url.encode
        url_decode = url.decode

        -- Regex helpers
        function regex_replace(text, pattern, replacement)
            if not text or not pattern then return text or "" end
            local ok, re = pcall(regex.compile, pattern)
            if not ok or not re then return text end
            return re:replace(text, replacement or "", true)
        end

        function regex_match(text, pattern)
            if not text or not pattern then return nil end
            local ok, re = pcall(regex.compile, pattern)
            if not ok or not re then return nil end
            return re:match(text)
        end

        -- Network HTTP Helpers
        function http_get(url, headers)
            local ok, Jni = pcall(require, "jni")
            if ok and Jni then
                local okBridge, Bridge = pcall(Jni.import, "my/noveldokusha/scraper/NetworkBridge")
                if okBridge and Bridge then
                    local res = Bridge:get(url)
                    return {
                        success = res.success == true,
                        body = tostring(res.body or ""),
                        statusCode = tonumber(res.statusCode) or 500
                    }
                end
            end
            return { success = false, body = "", statusCode = 500 }
        end

        function http_post(url, body, headers)
            local ok, Jni = pcall(require, "jni")
            if ok and Jni then
                local okBridge, Bridge = pcall(Jni.import, "my/noveldokusha/scraper/NetworkBridge")
                if okBridge and Bridge then
                    local res = Bridge:post(url, tostring(body or ""))
                    return {
                        success = res.success == true,
                        body = tostring(res.body or ""),
                        statusCode = tonumber(res.statusCode) or 500
                    }
                end
            end
            return { success = false, body = "", statusCode = 500 }
        end

        -- HTML helpers
        local function make_el(node, fullHtml)
            if not node then return nil end
            local outer = (node.outerHtml and node:outerHtml()) or (node.html and node:html()) or fullHtml or ""
            local el = {
                text = node:text() or "",
                html = outer,
                href = node:attr("href") or "",
                src = node:attr("src") or node:attr("data-src") or ""
            }
            setmetatable(el, {
                __index = function(t, k)
                    if k == "attr" then
                        return function(self, attr_name)
                            return node:attr(attr_name) or ""
                        end
                    end
                    return rawget(t, k)
                end
            })
            return el
        end

        function html_select(body, selector)
            if not body or not selector then return {} end
            local bodyStr = type(body) == "table" and body.html or tostring(body)
            local doc = html.parse(bodyStr)
            local nodes = doc:querySelectorAll(selector)
            local res = {}
            for i, node in ipairs(nodes) do
                table.insert(res, make_el(node, bodyStr))
            end
            return res
        end

        function html_select_first(body, selector)
            if not body or not selector then return nil end
            local bodyStr = type(body) == "table" and body.html or tostring(body)
            local doc = html.parse(bodyStr)
            local node = doc:querySelector(selector)
            return make_el(node, bodyStr)
        end

        function html_attr(html_str, selector, attr_name)
            if not html_str or not selector or not attr_name then return "" end
            local bodyStr = type(html_str) == "table" and html_str.html or tostring(html_str)
            local doc = html.parse(bodyStr)
            if selector == "*" then
                local root = doc:querySelector("*")
                if not root then return "" end
                return root:attr(attr_name) or ""
            end
            local node = doc:querySelector(selector)
            if not node then return "" end
            return node:attr(attr_name) or ""
        end

        function html_text(html_str, selector)
            if not html_str then return "" end
            local bodyStr = type(html_str) == "table" and html_str.html or tostring(html_str)
            local doc = html.parse(bodyStr)
            if not selector or selector == "" or selector == "*" then
                return doc:text() or ""
            end
            local node = doc:querySelector(selector)
            if not node then return "" end
            return node:text() or ""
        end

        function html_remove(body, ...)
            if not body then return "" end
            local bodyStr = type(body) == "table" and body.html or tostring(body)
            local doc = html.parse(bodyStr)
            local selectors = {...}
            for _, sel in ipairs(selectors) do
                if sel and sel ~= "" then
                    local nodes = doc:querySelectorAll(sel)
                    for _, node in ipairs(nodes) do
                        if node.remove then node:remove() end
                    end
                end
            end
            return (doc.outerHtml and doc:outerHtml()) or (doc.html and doc:html()) or bodyStr
        end

        function log_error(msg)
            print("LUAU ERROR: " .. tostring(msg or ""))
        end
    )";

    char *shim_err = NULL;
    luau_execute_script(runtime, COMPATIBILITY_SHIM_SCRIPT, "=compat_shim", 0.0, &shim_err);
    if (shim_err) {
        free(shim_err);
    }

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

int luau_eval_json(luau_runtime_t *runtime, const char *source, double timeout_seconds, char **out_json, char **out_error) {
    if (!runtime || !source) {
        if (out_error) *out_error = strdup("Invalid arguments");
        return -1;
    }

    lua_CompileOptions compile_options;
    memset(&compile_options, 0, sizeof(compile_options));
    compile_options.optimizationLevel = 1;
    compile_options.debugLevel = 1;

    size_t bytecode_size = 0;
    char *bytecode = luau_compile(source, strlen(source), &compile_options, &bytecode_size);
    if (!bytecode) {
        if (out_error) *out_error = strdup("Compilation failed");
        return -1;
    }

    int status = luau_load(runtime->L, "=eval", bytecode, bytecode_size, 0);
    free(bytecode);

    if (status != 0) {
        if (out_error) {
            const char *err = lua_tostring(runtime->L, -1);
            *out_error = strdup(err ? err : "Syntax error");
        }
        lua_pop(runtime->L, 1);
        return status;
    }

    runtime->timeout_seconds = timeout_seconds;
    runtime->start_time = get_time_seconds();
    runtime->is_running = true;

    status = lua_pcall(runtime->L, 0, 1, 0);

    runtime->is_running = false;

    if (status != 0) {
        if (out_error) {
            const char *err = lua_tostring(runtime->L, -1);
            *out_error = strdup(err ? err : "Runtime error");
        }
        lua_pop(runtime->L, 1);
        return status;
    }

    if (lua_isstring(runtime->L, -1)) {
        size_t len = 0;
        const char *str = lua_tolstring(runtime->L, -1, &len);
        if (out_json && str) *out_json = strdup(str);
    }
    lua_pop(runtime->L, 1);
    return 0;
}

