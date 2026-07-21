#include "html_module.h"
#include "lualib.h"
#include "lexbor/html/parser.h"
#include "lexbor/html/interfaces/document.h"
#include "lexbor/dom/interfaces/element.h"
#include "lexbor/dom/interfaces/node.h"
#include "lexbor/dom/interfaces/document.h"
#include <stdlib.h>
#include <string.h>
#include <string>

static void doc_handle_retain(native_html_doc_handle_t *h) {
    h->refcount++;
}

static void doc_handle_release(native_html_doc_handle_t *h) {
    if (--h->refcount == 0) {
        lxb_html_document_destroy(h->lexbor_doc);
        free(h);
    }
}

// Check if node matches selector (.class, #id, tag)
static bool match_node(lxb_dom_node_t *node, const char *selector) {
    if (node->type != LXB_DOM_NODE_TYPE_ELEMENT) return false;
    lxb_dom_element_t *el = (lxb_dom_element_t*)node;

    if (selector[0] == '.') {
        const char *class_name = selector + 1;
        size_t len = 0;
        const lxb_char_t *val = lxb_dom_element_get_attribute(el, (const lxb_char_t*)"class", 5, &len);
        if (val) {
            std::string s((const char*)val, len);
            size_t pos = s.find(class_name);
            if (pos != std::string::npos) {
                bool left_ok = (pos == 0 || s[pos - 1] == ' ');
                bool right_ok = (pos + strlen(class_name) == s.length() || s[pos + strlen(class_name)] == ' ');
                if (left_ok && right_ok) return true;
            }
        }
    } else if (selector[0] == '#') {
        const char *id_val = selector + 1;
        size_t len = 0;
        const lxb_char_t *val = lxb_dom_element_get_attribute(el, (const lxb_char_t*)"id", 2, &len);
        if (val && len == strlen(id_val) && memcmp(val, id_val, len) == 0) {
            return true;
        }
    } else {
        size_t len = 0;
        const lxb_char_t *name = lxb_dom_element_qualified_name(el, &len);
        if (name && len == strlen(selector) && memcmp(name, selector, len) == 0) {
            return true;
        }
    }
    return false;
}

// DFS to find matching node
static lxb_dom_node_t* find_node_dfs(lxb_dom_node_t *node, const char *selector) {
    if (!node) return NULL;
    if (match_node(node, selector)) return node;

    lxb_dom_node_t *child = node->first_child;
    while (child) {
        lxb_dom_node_t *found = find_node_dfs(child, selector);
        if (found) return found;
        child = child->next;
    }
    return NULL;
}

// Helper to push html_node
static int push_html_node(lua_State *L, lxb_dom_node_t *node, native_html_doc_handle_t *handle) {
    if (!node) {
        lua_pushnil(L);
        return 1;
    }

    html_node_t *html_node = (html_node_t*)lua_newuserdata(L, sizeof(html_node_t));
    html_node->node = node;
    html_node->handle = handle;
    doc_handle_retain(handle);

    luaL_getmetatable(L, "LuauHtmlNode");
    lua_setmetatable(L, -2);
    return 1;
}

static int html_parse(lua_State *L) {
    const char *html_str = luaL_checkstring(L, 1);

    native_html_doc_handle_t *handle = (native_html_doc_handle_t*)malloc(sizeof(native_html_doc_handle_t));
    if (!handle) {
        luaL_error(L, "Failed to allocate document handle");
        return 0;
    }
    handle->refcount = 0;
    handle->lexbor_doc = lxb_html_document_create();
    if (!handle->lexbor_doc) {
        free(handle);
        luaL_error(L, "Failed to create Lexbor HTML document");
        return 0;
    }

    lxb_status_t status = lxb_html_document_parse(handle->lexbor_doc, (const lxb_char_t*)html_str, strlen(html_str));
    if (status != LXB_STATUS_OK) {
        lxb_html_document_destroy(handle->lexbor_doc);
        free(handle);
        luaL_error(L, "Failed to parse HTML string");
        return 0;
    }

    html_doc_t *html_doc = (html_doc_t*)lua_newuserdata(L, sizeof(html_doc_t));
    html_doc->handle = handle;
    doc_handle_retain(handle);

    luaL_getmetatable(L, "LuauHtmlDoc");
    lua_setmetatable(L, -2);
    return 1;
}

static int html_doc_gc(lua_State *L) {
    html_doc_t *html_doc = (html_doc_t*)lua_touserdata(L, 1);
    if (html_doc && html_doc->handle) {
        doc_handle_release(html_doc->handle);
        html_doc->handle = NULL;
    }
    return 0;
}

static int html_doc_querySelector(lua_State *L) {
    html_doc_t *html_doc = (html_doc_t*)luaL_checkudata(L, 1, "LuauHtmlDoc");
    const char *selector = luaL_checkstring(L, 2);

    lxb_dom_node_t *root = lxb_dom_document_root((lxb_dom_document_t*)html_doc->handle->lexbor_doc);
    lxb_dom_node_t *found = find_node_dfs(root, selector);

    return push_html_node(L, found, html_doc->handle);
}

static int html_node_gc(lua_State *L) {
    html_node_t *html_node = (html_node_t*)lua_touserdata(L, 1);
    if (html_node && html_node->handle) {
        doc_handle_release(html_node->handle);
        html_node->handle = NULL;
        html_node->node = NULL;
    }
    return 0;
}

static int html_node_querySelector(lua_State *L) {
    html_node_t *html_node = (html_node_t*)luaL_checkudata(L, 1, "LuauHtmlNode");
    const char *selector = luaL_checkstring(L, 2);

    lxb_dom_node_t *found = find_node_dfs(html_node->node, selector);

    return push_html_node(L, found, html_node->handle);
}

static int html_node_text(lua_State *L) {
    html_node_t *html_node = (html_node_t*)luaL_checkudata(L, 1, "LuauHtmlNode");
    size_t len = 0;
    lxb_char_t *text = lxb_dom_node_text_content(html_node->node, &len);
    if (text) {
        lua_pushlstring(L, (const char*)text, len);
    } else {
        lua_pushstring(L, "");
    }
    return 1;
}

static int html_node_tag(lua_State *L) {
    html_node_t *html_node = (html_node_t*)luaL_checkudata(L, 1, "LuauHtmlNode");
    if (html_node->node->type == LXB_DOM_NODE_TYPE_ELEMENT) {
        size_t len = 0;
        const lxb_char_t *name = lxb_dom_element_qualified_name((lxb_dom_element_t*)html_node->node, &len);
        if (name) {
            lua_pushlstring(L, (const char*)name, len);
            return 1;
        }
    }
    lua_pushstring(L, "");
    return 1;
}

static int html_node_attr(lua_State *L) {
    html_node_t *html_node = (html_node_t*)luaL_checkudata(L, 1, "LuauHtmlNode");
    const char *name = luaL_checkstring(L, 2);

    if (html_node->node->type == LXB_DOM_NODE_TYPE_ELEMENT) {
        size_t len = 0;
        const lxb_char_t *val = lxb_dom_element_get_attribute((lxb_dom_element_t*)html_node->node, (const lxb_char_t*)name, strlen(name), &len);
        if (val) {
            lua_pushlstring(L, (const char*)val, len);
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

int luaopen_html(lua_State *L) {
    // 1. Create metatable for LuauHtmlDoc
    luaL_newmetatable(L, "LuauHtmlDoc");
    lua_pushcfunction(L, html_doc_gc, "__gc");
    lua_setfield(L, -2, "__gc");
    
    lua_newtable(L);
    lua_pushcfunction(L, html_doc_querySelector, "querySelector");
    lua_setfield(L, -2, "querySelector");
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    // 2. Create metatable for LuauHtmlNode
    luaL_newmetatable(L, "LuauHtmlNode");
    lua_pushcfunction(L, html_node_gc, "__gc");
    lua_setfield(L, -2, "__gc");

    lua_newtable(L);
    lua_pushcfunction(L, html_node_querySelector, "querySelector");
    lua_setfield(L, -2, "querySelector");
    lua_pushcfunction(L, html_node_text, "text");
    lua_setfield(L, -2, "text");
    lua_pushcfunction(L, html_node_tag, "tag");
    lua_setfield(L, -2, "tag");
    lua_pushcfunction(L, html_node_attr, "attr");
    lua_setfield(L, -2, "attr");
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    // 3. Create module functions
    lua_newtable(L);
    lua_pushcfunction(L, html_parse, "parse");
    lua_setfield(L, -2, "parse");

    return 1;
}
