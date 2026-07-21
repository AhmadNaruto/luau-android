#ifndef HTML_MODULE_H
#define HTML_MODULE_H

#include "lua.h"
#include "lexbor/html/interfaces/document.h"
#include "lexbor/dom/interfaces/node.h"

// Refcounted document handle
typedef struct {
    lxb_html_document_t *lexbor_doc;
    int refcount;
} native_html_doc_handle_t;

// Document Userdata Layout
typedef struct {
    native_html_doc_handle_t *handle;
} html_doc_t;

// Node Userdata Layout
typedef struct {
    native_html_doc_handle_t *handle;
    lxb_dom_node_t *node;
} html_node_t;

int luaopen_html(lua_State *L);

#endif // HTML_MODULE_H
