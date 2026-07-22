#include "html_module.h"
#include "lualib.h"
#include "lexbor/html/parser.h"
#include "lexbor/html/serialize.h"
#include "lexbor/html/interfaces/document.h"
#include "lexbor/dom/interfaces/element.h"
#include "lexbor/dom/interfaces/node.h"
#include "lexbor/dom/interfaces/document.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <string>
#include <vector>

static void doc_handle_retain(native_html_doc_handle_t *h) {
    h->refcount++;
}

static void doc_handle_release(native_html_doc_handle_t *h) {
    if (--h->refcount == 0) {
        lxb_html_document_destroy(h->lexbor_doc);
        free(h);
    }
}

struct lexbor_serialize_ctx_t {
    std::string str;
};

static lxb_status_t lexbor_serialize_callback(const lxb_char_t *data, size_t len, void *ctx) {
    lexbor_serialize_ctx_t *s = (lexbor_serialize_ctx_t*)ctx;
    s->str.append((const char*)data, len);
    return LXB_STATUS_OK;
}

static std::string serialize_node(lxb_dom_node_t *node) {
    if (!node) return "";
    lexbor_serialize_ctx_t s;
    lxb_html_serialize_deep_cb(node, lexbor_serialize_callback, &s);
    return s.str;
}

static bool match_compound_token(lxb_dom_element_t *el, const std::string &token) {
    if (!el || token.empty() || token == "*") return true;

    size_t not_pos = token.find(":not(");
    if (not_pos != std::string::npos) {
        size_t close_pos = token.find(')', not_pos);
        if (close_pos != std::string::npos) {
            std::string main_part = token.substr(0, not_pos);
            std::string not_part = token.substr(not_pos + 5, close_pos - (not_pos + 5));
            if (!main_part.empty() && !match_compound_token(el, main_part)) return false;
            if (match_compound_token(el, not_part)) return false;
            return true;
        }
    }

    size_t i = 0;
    size_t len = token.length();

    if (token[0] != '.' && token[0] != '#' && token[0] != '[') {
        size_t end_tag = token.find_first_of(".#[", 0);
        std::string tag_name = (end_tag == std::string::npos) ? token : token.substr(0, end_tag);
        size_t vlen = 0;
        const lxb_char_t *el_tag = lxb_dom_element_qualified_name(el, &vlen);
        if (!el_tag) return false;
        std::string el_tag_str((const char*)el_tag, vlen);
        if (strcasecmp(el_tag_str.c_str(), tag_name.c_str()) != 0) return false;
        i = (end_tag == std::string::npos) ? len : end_tag;
    }

    while (i < len) {
        if (token[i] == '.') {
            i++;
            size_t start = i;
            while (i < len && token[i] != '.' && token[i] != '#' && token[i] != '[') i++;
            std::string cls_name = token.substr(start, i - start);
            
            size_t vlen = 0;
            const lxb_char_t *cls_attr = lxb_dom_element_get_attribute(el, (const lxb_char_t*)"class", 5, &vlen);
            if (!cls_attr || vlen == 0) return false;
            
            std::string class_str((const char*)cls_attr, vlen);
            bool found_class = false;
            size_t ci = 0;
            while (ci < vlen) {
                while (ci < vlen && class_str[ci] == ' ') ci++;
                size_t cstart = ci;
                while (ci < vlen && class_str[ci] != ' ') ci++;
                size_t clen = ci - cstart;
                if (clen == cls_name.length() && memcmp(class_str.c_str() + cstart, cls_name.c_str(), clen) == 0) {
                    found_class = true;
                    break;
                }
            }
            if (!found_class) return false;
        } else if (token[i] == '#') {
            i++;
            size_t start = i;
            while (i < len && token[i] != '.' && token[i] != '#' && token[i] != '[') i++;
            std::string id_val = token.substr(start, i - start);
            
            size_t vlen = 0;
            const lxb_char_t *id_attr = lxb_dom_element_get_attribute(el, (const lxb_char_t*)"id", 2, &vlen);
            if (!id_attr || vlen != id_val.length() || memcmp(id_attr, id_val.c_str(), vlen) != 0) return false;
        } else if (token[i] == '[') {
            i++;
            size_t close_bracket = token.find(']', i);
            if (close_bracket == std::string::npos) return false;
            std::string attr_expr = token.substr(i, close_bracket - i);
            i = close_bracket + 1;
            
            size_t eq_pos = attr_expr.find('=');
            if (eq_pos == std::string::npos) {
                size_t vlen = 0;
                const lxb_char_t *val = lxb_dom_element_get_attribute(el, (const lxb_char_t*)attr_expr.c_str(), attr_expr.length(), &vlen);
                if (!val) return false;
            } else {
                char op = '=';
                std::string attr_name = attr_expr.substr(0, eq_pos);
                if (eq_pos > 0 && (attr_expr[eq_pos - 1] == '^' || attr_expr[eq_pos - 1] == '$' || attr_expr[eq_pos - 1] == '*')) {
                    op = attr_expr[eq_pos - 1];
                    attr_name = attr_expr.substr(0, eq_pos - 1);
                }
                std::string expected_val = attr_expr.substr(eq_pos + 1);
                if (!expected_val.empty() && (expected_val.front() == '"' || expected_val.front() == '\'')) expected_val.erase(0, 1);
                if (!expected_val.empty() && (expected_val.back() == '"' || expected_val.back() == '\'')) expected_val.pop_back();

                size_t vlen = 0;
                const lxb_char_t *val = lxb_dom_element_get_attribute(el, (const lxb_char_t*)attr_name.c_str(), attr_name.length(), &vlen);
                if (!val) return false;
                std::string actual_val((const char*)val, vlen);

                if (op == '=') {
                    if (actual_val != expected_val) return false;
                } else if (op == '^') {
                    if (actual_val.rfind(expected_val, 0) != 0) return false;
                } else if (op == '$') {
                    if (actual_val.length() < expected_val.length() || actual_val.compare(actual_val.length() - expected_val.length(), expected_val.length(), expected_val) != 0) return false;
                } else if (op == '*') {
                    if (actual_val.find(expected_val) == std::string::npos) return false;
                }
            }
        } else {
            i++;
        }
    }
    return true;
}

struct selector_token_t {
    std::string token;
    char combinator;
};

static std::vector<selector_token_t> parse_selector_chain(const std::string &selector) {
    std::vector<selector_token_t> chain;
    size_t i = 0;
    size_t len = selector.length();
    char current_comb = ' ';

    while (i < len) {
        while (i < len && (selector[i] == ' ' || selector[i] == '\t')) i++;
        if (i >= len) break;

        if (selector[i] == '>') {
            current_comb = '>';
            i++;
            while (i < len && (selector[i] == ' ' || selector[i] == '\t')) i++;
        }

        size_t start = i;
        while (i < len && selector[i] != ' ' && selector[i] != '\t' && selector[i] != '>') {
            if (selector[i] == '[') {
                while (i < len && selector[i] != ']') i++;
                if (i < len) i++;
            } else {
                i++;
            }
        }

        std::string tok = selector.substr(start, i - start);
        if (!tok.empty()) {
            chain.push_back({tok, current_comb});
            current_comb = ' ';
        }
    }
    return chain;
}

static bool match_node_chain(lxb_dom_node_t *node, const std::vector<selector_token_t> &chain) {
    if (!node || node->type != LXB_DOM_NODE_TYPE_ELEMENT || chain.empty()) return false;

    int idx = (int)chain.size() - 1;
    if (!match_compound_token((lxb_dom_element_t*)node, chain[idx].token)) {
        return false;
    }

    lxb_dom_node_t *curr_node = node;
    idx--;

    while (idx >= 0) {
        char comb = chain[idx + 1].combinator;
        if (comb == '>') {
            lxb_dom_node_t *parent = curr_node->parent;
            if (!parent || parent->type != LXB_DOM_NODE_TYPE_ELEMENT) return false;
            if (!match_compound_token((lxb_dom_element_t*)parent, chain[idx].token)) return false;
            curr_node = parent;
        } else {
            lxb_dom_node_t *parent = curr_node->parent;
            bool found_ancestor = false;
            while (parent) {
                if (parent->type == LXB_DOM_NODE_TYPE_ELEMENT) {
                    if (match_compound_token((lxb_dom_element_t*)parent, chain[idx].token)) {
                        curr_node = parent;
                        found_ancestor = true;
                        break;
                    }
                }
                parent = parent->parent;
            }
            if (!found_ancestor) return false;
        }
        idx--;
    }
    return true;
}

static bool match_node(lxb_dom_node_t *node, const char *selector) {
    if (!node || node->type != LXB_DOM_NODE_TYPE_ELEMENT || !selector || !*selector) return false;

    std::string sel_str(selector);
    size_t start = 0;
    while (start < sel_str.length()) {
        size_t comma = sel_str.find(',', start);
        std::string sub_sel = (comma == std::string::npos) ? sel_str.substr(start) : sel_str.substr(start, comma - start);
        
        size_t s = sub_sel.find_first_not_of(" \t");
        size_t e = sub_sel.find_last_not_of(" \t");
        if (s != std::string::npos && e != std::string::npos) {
            sub_sel = sub_sel.substr(s, e - s + 1);
            auto chain = parse_selector_chain(sub_sel);
            if (match_node_chain(node, chain)) return true;
        }

        if (comma == std::string::npos) break;
        start = comma + 1;
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

static int html_node_outerHtml(lua_State *L) {
    html_node_t *html_node = (html_node_t*)luaL_checkudata(L, 1, "LuauHtmlNode");
    if (html_node && html_node->node) {
        std::string html_str = serialize_node(html_node->node);
        lua_pushlstring(L, html_str.c_str(), html_str.length());
        return 1;
    }
    lua_pushstring(L, "");
    return 1;
}

static int html_doc_outerHtml(lua_State *L) {
    html_doc_t *html_doc = (html_doc_t*)luaL_checkudata(L, 1, "LuauHtmlDoc");
    if (html_doc && html_doc->handle && html_doc->handle->lexbor_doc) {
        lxb_dom_node_t *root = lxb_dom_document_root((lxb_dom_document_t*)html_doc->handle->lexbor_doc);
        std::string html_str = serialize_node(root);
        lua_pushlstring(L, html_str.c_str(), html_str.length());
        return 1;
    }
    lua_pushstring(L, "");
    return 1;
}

static int html_node_remove(lua_State *L) {
    html_node_t *html_node = (html_node_t*)luaL_checkudata(L, 1, "LuauHtmlNode");
    if (html_node && html_node->node) {
        lxb_dom_node_remove(html_node->node);
    }
    return 0;
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
    lua_pushcfunction(L, html_doc_outerHtml, "html");
    lua_setfield(L, -2, "html");
    lua_pushcfunction(L, html_doc_outerHtml, "outerHtml");
    lua_setfield(L, -2, "outerHtml");
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
    lua_pushcfunction(L, html_node_outerHtml, "html");
    lua_setfield(L, -2, "html");
    lua_pushcfunction(L, html_node_outerHtml, "outerHtml");
    lua_setfield(L, -2, "outerHtml");
    lua_pushcfunction(L, html_node_remove, "remove");
    lua_setfield(L, -2, "remove");
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    // 3. Create module functions
    lua_newtable(L);
    lua_pushcfunction(L, html_parse, "parse");
    lua_setfield(L, -2, "parse");

    return 1;
}
