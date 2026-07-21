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

static bool match_single_selector(lxb_dom_element_t *el, const char *sel, size_t sel_len) {
    if (sel_len == 0) return false;
    if (sel[0] == '.') {
        size_t vlen = 0;
        const lxb_char_t *cls = lxb_dom_element_get_attribute(el, (const lxb_char_t*)"class", 5, &vlen);
        if (!cls || vlen == 0) return false;
        const char *class_str = (const char*)cls;
        const char *target = sel + 1;
        size_t tlen = sel_len - 1;
        size_t ci = 0;
        while (ci < vlen) {
            while (ci < vlen && class_str[ci] == ' ') ci++;
            size_t start = ci;
            while (ci < vlen && class_str[ci] != ' ') ci++;
            size_t wlen = ci - start;
            if (wlen == tlen && memcmp(class_str + start, target, tlen) == 0) {
                return true;
            }
        }
        return false;
    } else if (sel[0] == '#') {
        size_t vlen = 0;
        const lxb_char_t *id_attr = lxb_dom_element_get_attribute(el, (const lxb_char_t*)"id", 2, &vlen);
        if (!id_attr || vlen == 0) return false;
        return (vlen == sel_len - 1 && memcmp(id_attr, sel + 1, vlen) == 0);
    } else if (sel[0] == '[') {
        const char *close_bracket = (const char*)memchr(sel, ']', sel_len);
        if (!close_bracket) return false;
        size_t attr_len = close_bracket - sel - 1;
        const char *attr_name = sel + 1;
        size_t vlen = 0;
        const lxb_char_t *attr_val = lxb_dom_element_get_attribute(el, (const lxb_char_t*)attr_name, attr_len, &vlen);
        return (attr_val != NULL);
    } else {
        const char *bracket = (const char*)memchr(sel, '[', sel_len);
        size_t tag_len = bracket ? (size_t)(bracket - sel) : sel_len;
        size_t vlen = 0;
        const lxb_char_t *name = lxb_dom_element_qualified_name(el, &vlen);
        if (!name || vlen != tag_len || memcmp(name, sel, tag_len) != 0) {
            return false;
        }
        if (bracket) {
            const char *close_bracket = (const char*)memchr(bracket, ']', sel_len - tag_len);
            if (!close_bracket) return false;
            size_t attr_len = close_bracket - bracket - 1;
            const char *attr_name = bracket + 1;
            size_t aval_len = 0;
            const lxb_char_t *attr_val = lxb_dom_element_get_attribute(el, (const lxb_char_t*)attr_name, attr_len, &aval_len);
            return (attr_val != NULL && aval_len > 0);
        }
        return true;
    }
}

static bool match_node(lxb_dom_node_t *node, const char *selector) {
    if (!node || node->type != LXB_DOM_NODE_TYPE_ELEMENT) return false;
    lxb_dom_element_t *el = (lxb_dom_element_t*)node;

    const char *last_space = strrchr(selector, ' ');
    if (!last_space) {
        return match_single_selector(el, selector, strlen(selector));
    }

    const char *target_sel = last_space + 1;
    if (*target_sel == '>') target_sel++;
    while (*target_sel == ' ') target_sel++;
    if (*target_sel == '\0') return false;

    if (!match_single_selector(el, target_sel, strlen(target_sel))) {
        return false;
    }

    const char *first_token = selector;
    size_t first_len = last_space - selector;
    while (first_len > 0 && (first_token[first_len - 1] == ' ' || first_token[first_len - 1] == '>')) {
        first_len--;
    }

    lxb_dom_node_t *parent = node->parent;
    while (parent) {
        if (parent->type == LXB_DOM_NODE_TYPE_ELEMENT) {
            if (match_single_selector((lxb_dom_element_t*)parent, first_token, first_len)) {
                return true;
            }
        }
        parent = parent->parent;
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

static void find_all_nodes_dfs(lxb_dom_node_t *node, const char *selector, lua_State *L, native_html_doc_handle_t *handle, int *count) {
    if (!node) return;
    if (match_node(node, selector)) {
        push_html_node(L, node, handle);
        lua_rawseti(L, -2, ++(*count));
    }

    lxb_dom_node_t *child = node->first_child;
    while (child) {
        find_all_nodes_dfs(child, selector, L, handle, count);
        child = child->next;
    }
}

static int html_doc_querySelectorAll(lua_State *L) {
    html_doc_t *html_doc = (html_doc_t*)luaL_checkudata(L, 1, "LuauHtmlDoc");
    const char *selector = luaL_checkstring(L, 2);

    lxb_dom_node_t *root = lxb_dom_document_root((lxb_dom_document_t*)html_doc->handle->lexbor_doc);
    lua_newtable(L);
    int count = 0;
    find_all_nodes_dfs(root, selector, L, html_doc->handle, &count);
    return 1;
}

static int html_node_querySelectorAll(lua_State *L) {
    html_node_t *html_node = (html_node_t*)luaL_checkudata(L, 1, "LuauHtmlNode");
    const char *selector = luaL_checkstring(L, 2);

    lua_newtable(L);
    int count = 0;
    find_all_nodes_dfs(html_node->node, selector, L, html_node->handle, &count);
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
    lua_pushcfunction(L, html_doc_querySelectorAll, "querySelectorAll");
    lua_setfield(L, -2, "querySelectorAll");
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    // 2. Create metatable for LuauHtmlNode
    luaL_newmetatable(L, "LuauHtmlNode");
    lua_pushcfunction(L, html_node_gc, "__gc");
    lua_setfield(L, -2, "__gc");

    lua_newtable(L);
    lua_pushcfunction(L, html_node_querySelector, "querySelector");
    lua_setfield(L, -2, "querySelector");
    lua_pushcfunction(L, html_node_querySelectorAll, "querySelectorAll");
    lua_setfield(L, -2, "querySelectorAll");
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
