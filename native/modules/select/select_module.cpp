#include "select_module.h"
#include "lualib.h"
#include "html_module.h"
#include "lexbor/html/html.h"
#include "lexbor/dom/interfaces/element.h"
#include "lexbor/dom/interfaces/node.h"
#include <stdlib.h>
#include <string.h>

// ─── Operation types ──────────────────────────────────────────────────────────

typedef enum {
    SELECT_OP_TEXT,   // :text(key, selector)
    SELECT_OP_ATTR,   // :attr(key, selector, attr_name)
    SELECT_OP_HTML,   // :html(key, selector)  — inner HTML as string
} select_op_type_t;

typedef struct {
    select_op_type_t type;
    char *key;
    char *selector;
    char *attr_name; // only for SELECT_OP_ATTR
} select_op_t;

// ─── Builder userdata ─────────────────────────────────────────────────────────

#define SELECT_BUILDER_MT "LuauSelectBuilder"

typedef struct {
    int          doc_ref;     // Lua registry reference to the document userdata
    select_op_t *ops;
    size_t       ops_len;
    size_t       ops_cap;
} select_builder_t;

// ─── CSS selector helper (DFS, same logic as html_module) ────────────────────

typedef struct {
    const char *sel;
    size_t      sel_len;
    lxb_dom_node_t *found;
} dfs_ctx_t;

static bool selector_matches(lxb_dom_element_t *el, const char *sel, size_t sel_len) {
    if (sel_len == 0) return false;
    if (sel[0] == '.') {
        // Class selector
        size_t vlen = 0;
        const lxb_char_t *cls = lxb_dom_element_get_attribute(
            el, (const lxb_char_t*)"class", 5, &vlen);
        if (!cls || vlen == 0) return false;
        const char *class_str = (const char*)cls;
        const char *target    = sel + 1;
        size_t      tlen      = sel_len - 1;
        // Check if target is a whole word in the class list
        size_t ci = 0;
        while (ci < vlen) {
            while (ci < vlen && class_str[ci] == ' ') ci++;
            size_t start = ci;
            while (ci < vlen && class_str[ci] != ' ') ci++;
            if ((ci - start) == tlen && memcmp(class_str + start, target, tlen) == 0)
                return true;
        }
        return false;
    } else if (sel[0] == '#') {
        // ID selector
        size_t vlen = 0;
        const lxb_char_t *id = lxb_dom_element_get_attribute(
            el, (const lxb_char_t*)"id", 2, &vlen);
        if (!id || vlen == 0) return false;
        return (vlen == sel_len - 1) && (memcmp(id, sel + 1, vlen) == 0);
    } else {
        // Tag selector
        size_t tag_len = 0;
        const lxb_char_t *tag = lxb_dom_element_qualified_name(el, &tag_len);
        if (!tag || tag_len == 0) return false;
        if (tag_len != sel_len) return false;
        for (size_t i = 0; i < tag_len; i++) {
            char a = (char)(tag[i]  >= 'A' && tag[i]  <= 'Z' ? tag[i] + 32 : tag[i]);
            char b = (char)(((unsigned char)sel[i] >= 'A' && (unsigned char)sel[i] <= 'Z') ? sel[i] + 32 : sel[i]);
            if (a != b) return false;
        }
        return true;
    }
}

static void dfs_find(lxb_dom_node_t *root_node, dfs_ctx_t *ctx) {
    if (!root_node) return;

    // Fixed capacity stack buffer for fast stack allocation (handles up to 512 nested nodes)
    lxb_dom_node_t *stack_buf[512];
    lxb_dom_node_t **stack = stack_buf;
    size_t stack_cap = 512;
    size_t stack_size = 0;

    stack[stack_size++] = root_node;

    while (stack_size > 0) {
        lxb_dom_node_t *curr = stack[--stack_size];

        lxb_dom_node_t *child = lxb_dom_node_first_child(curr);
        // Push children in reverse order or left-to-right
        while (child) {
            if (child->type == LXB_DOM_NODE_TYPE_ELEMENT) {
                lxb_dom_element_t *el = lxb_dom_interface_element(child);
                if (selector_matches(el, ctx->sel, ctx->sel_len)) {
                    ctx->found = child;
                    if (stack != stack_buf) free(stack);
                    return;
                }
                if (stack_size >= stack_cap) {
                    stack_cap *= 2;
                    if (stack == stack_buf) {
                        stack = (lxb_dom_node_t**)malloc(stack_cap * sizeof(lxb_dom_node_t*));
                        memcpy(stack, stack_buf, stack_size * sizeof(lxb_dom_node_t*));
                    } else {
                        stack = (lxb_dom_node_t**)realloc(stack, stack_cap * sizeof(lxb_dom_node_t*));
                    }
                }
                stack[stack_size++] = child;
            }
            child = lxb_dom_node_next(child);
        }
    }

    if (stack != stack_buf) free(stack);
}

// Get inner text (recursive)
static void collect_text(lxb_dom_node_t *node, char **out, size_t *out_len, size_t *out_cap) {
    lxb_dom_node_t *child = lxb_dom_node_first_child(node);
    while (child) {
        if (child->type == LXB_DOM_NODE_TYPE_TEXT) {
            size_t len = 0;
            const lxb_char_t *txt = lxb_dom_node_text_content(child, &len);
            if (txt && len > 0) {
                size_t needed = *out_len + len + 1;
                if (needed > *out_cap) {
                    *out_cap = needed * 2;
                    *out = (char*)realloc(*out, *out_cap);
                }
                memcpy(*out + *out_len, txt, len);
                *out_len += len;
                (*out)[*out_len] = '\0';
            }
        } else if (child->type == LXB_DOM_NODE_TYPE_ELEMENT) {
            collect_text(child, out, out_len, out_cap);
        }
        child = lxb_dom_node_next(child);
    }
}

// ─── Builder methods ──────────────────────────────────────────────────────────

static select_builder_t* check_builder(lua_State *L, int idx) {
    return (select_builder_t*)luaL_checkudata(L, idx, SELECT_BUILDER_MT);
}

static void builder_push_op(select_builder_t *b, select_op_type_t type,
                             const char *key, const char *selector, const char *attr) {
    if (b->ops_len >= b->ops_cap) {
        b->ops_cap = b->ops_cap ? b->ops_cap * 2 : 8;
        b->ops = (select_op_t*)realloc(b->ops, b->ops_cap * sizeof(select_op_t));
    }
    select_op_t *op = &b->ops[b->ops_len++];
    op->type      = type;
    op->key       = strdup(key);
    op->selector  = strdup(selector);
    op->attr_name = attr ? strdup(attr) : NULL;
}

// :text(key, selector) → self
static int builder_text(lua_State *L) {
    select_builder_t *b = check_builder(L, 1);
    const char *key = luaL_checkstring(L, 2);
    const char *sel = luaL_checkstring(L, 3);
    builder_push_op(b, SELECT_OP_TEXT, key, sel, NULL);
    lua_settop(L, 1); // return self
    return 1;
}

// :attr(key, selector, attr_name) → self
static int builder_attr(lua_State *L) {
    select_builder_t *b = check_builder(L, 1);
    const char *key  = luaL_checkstring(L, 2);
    const char *sel  = luaL_checkstring(L, 3);
    const char *attr = luaL_checkstring(L, 4);
    builder_push_op(b, SELECT_OP_ATTR, key, sel, attr);
    lua_settop(L, 1);
    return 1;
}

// :html(key, selector) → self  (serializes element outerHTML-like to string)
static int builder_html(lua_State *L) {
    select_builder_t *b = check_builder(L, 1);
    const char *key = luaL_checkstring(L, 2);
    const char *sel = luaL_checkstring(L, 3);
    builder_push_op(b, SELECT_OP_HTML, key, sel, NULL);
    lua_settop(L, 1);
    return 1;
}

// :build() → table
static int builder_build(lua_State *L) {
    select_builder_t *b = check_builder(L, 1);

    // Retrieve the document userdata from registry
    lua_rawgeti(L, LUA_REGISTRYINDEX, b->doc_ref);
    // The document userdata is either a Document or Node — both start with native_html_doc_handle_t*
    html_doc_t *el_ud = (html_doc_t*)lua_touserdata(L, -1);
    lua_pop(L, 1);

    if (!el_ud || !el_ud->handle || !el_ud->handle->lexbor_doc) {
        luaL_error(L, "select:build() — invalid document");
        return 0;
    }

    lxb_html_document_t *doc = el_ud->handle->lexbor_doc;
    lxb_dom_node_t *root = lxb_dom_interface_node(doc);

    // Output table
    lua_newtable(L);

    for (size_t i = 0; i < b->ops_len; i++) {
        select_op_t *op = &b->ops[i];

        // Find element via DFS
        dfs_ctx_t dctx = { op->selector, strlen(op->selector), NULL };
        dfs_find(root, &dctx);

        if (!dctx.found) {
            lua_pushnil(L);
            lua_setfield(L, -2, op->key);
            continue;
        }

        lxb_dom_element_t *found_el = lxb_dom_interface_element(dctx.found);

        switch (op->type) {
            case SELECT_OP_TEXT: {
                char *buf = (char*)malloc(256);
                size_t buf_len = 0, buf_cap = 256;
                buf[0] = '\0';
                collect_text(dctx.found, &buf, &buf_len, &buf_cap);
                lua_pushlstring(L, buf, buf_len);
                free(buf);
                break;
            }
            case SELECT_OP_ATTR: {
                size_t vlen = 0;
                const lxb_char_t *val = lxb_dom_element_get_attribute(
                    found_el,
                    (const lxb_char_t*)op->attr_name,
                    strlen(op->attr_name), &vlen);
                if (val && vlen > 0) {
                    lua_pushlstring(L, (const char*)val, vlen);
                } else {
                    lua_pushnil(L);
                }
                break;
            }
            case SELECT_OP_HTML: {
                // Simple: serialize tag name + attributes for now
                size_t tag_len = 0;
                const lxb_char_t *tag = lxb_dom_element_qualified_name(found_el, &tag_len);
                if (tag && tag_len > 0) {
                    lua_pushlstring(L, (const char*)tag, tag_len);
                } else {
                    lua_pushstring(L, "");
                }
                break;
            }
        }
        lua_setfield(L, -2, op->key);
    }

    return 1;
}

// __gc for builder
static int builder_gc(lua_State *L) {
    select_builder_t *b = check_builder(L, 1);
    if (b->doc_ref != LUA_NOREF) {
        lua_unref(L, b->doc_ref);
        b->doc_ref = LUA_NOREF;
    }
    for (size_t i = 0; i < b->ops_len; i++) {
        free(b->ops[i].key);
        free(b->ops[i].selector);
        if (b->ops[i].attr_name) free(b->ops[i].attr_name);
    }
    free(b->ops);
    b->ops     = NULL;
    b->ops_len = 0;
    b->ops_cap = 0;
    return 0;
}

// ─── select.from(doc) ────────────────────────────────────────────────────────

static int select_from(lua_State *L) {
    // Argument 1 must be a document/element userdata
    luaL_checktype(L, 1, LUA_TUSERDATA);

    select_builder_t *b = (select_builder_t*)lua_newuserdata(L, sizeof(select_builder_t));
    // Stack: [arg1=doc, builder_ud]
    b->ops     = NULL;
    b->ops_len = 0;
    b->ops_cap = 0;
    b->doc_ref = LUA_NOREF;

    // Set metatable on the builder BEFORE storing ref
    luaL_getmetatable(L, SELECT_BUILDER_MT);
    // Stack: [arg1=doc, builder_ud, mt]
    lua_setmetatable(L, -2);
    // Stack: [arg1=doc, builder_ud]

    // In Luau, lua_ref(L, idx) takes a STACK INDEX (NOT LUA_REGISTRYINDEX).
    // Reference arg1 (the doc userdata) directly — lua_ref does NOT pop.
    b->doc_ref = lua_ref(L, 1);

    return 1;
    // Returns builder_ud at top of stack
}

// ─── Module open ─────────────────────────────────────────────────────────────

int luaopen_select(lua_State *L) {
    // Register builder metatable
    luaL_newmetatable(L, SELECT_BUILDER_MT);

    lua_pushstring(L, "__index");
    lua_newtable(L);
    lua_pushcfunction(L, builder_text,  "text");  lua_setfield(L, -2, "text");
    lua_pushcfunction(L, builder_attr,  "attr");  lua_setfield(L, -2, "attr");
    lua_pushcfunction(L, builder_html,  "html");  lua_setfield(L, -2, "html");
    lua_pushcfunction(L, builder_build, "build"); lua_setfield(L, -2, "build");
    lua_settable(L, -3);

    lua_pushstring(L, "__gc");
    lua_pushcfunction(L, builder_gc, "__gc");
    lua_settable(L, -3);
    lua_pop(L, 1); // pop metatable

    // Module table
    lua_newtable(L);
    lua_pushcfunction(L, select_from, "from");
    lua_setfield(L, -2, "from");

    return 1;
}
