# HTML Module Memory Model — Document/Element Refcounting

## Problem
"Element references Document memory" is unsafe on its own: if a Lua script
drops `doc` but keeps an `Element` returned by `doc:select(...)`, GC could
free the Lexbor tree while the Element still points into it → use-after-free.

## Fix: shared ownership handle
Neither `Document` nor `Element` userdata own the Lexbor tree directly.
Both hold a pointer to a small shared **handle** that is refcounted. The
tree is freed only when the handle's refcount reaches zero — i.e. when the
*last* Lua-visible object referencing it (Document or any Element/Elements)
has been collected, regardless of order.

```c
typedef struct {
    lxb_html_document_t *lexbor_doc;
    int refcount;   // plain int: one VM per thread, no shared mutable state
} native_html_doc_handle_t;

static void doc_handle_retain(native_html_doc_handle_t *h) {
    h->refcount++;
}

static void doc_handle_release(native_html_doc_handle_t *h) {
    if (--h->refcount == 0) {
        lxb_html_document_destroy(h->lexbor_doc);
        free(h);
    }
}
```

### Document userdata
```c
typedef struct {
    native_html_doc_handle_t *handle;
} lua_html_document_ud;

// on html.parse(data):
//   handle = alloc, refcount = 0
//   doc_ud->handle = handle; doc_handle_retain(handle);   // Document's own ref

// Document __gc:
static int document_gc(lua_State *L) {
    lua_html_document_ud *ud = ...;
    doc_handle_release(ud->handle);
    return 0;
}
```

### Element / Elements userdata
```c
typedef struct {
    native_html_doc_handle_t *handle; // shared, not owned exclusively
    lxb_dom_node_t *node;             // valid as long as handle->refcount > 0
} lua_html_element_ud;

// every time select()/child()/etc. returns a new Element/Elements userdata:
//   elem_ud->handle = doc_handle; doc_handle_retain(doc_handle);

// Element __gc:
static int element_gc(lua_State *L) {
    lua_html_element_ud *ud = ...;
    doc_handle_release(ud->handle);
    return 0;
}
```

For an `Elements` collection, retain **once for the whole collection** (not
once per contained node) — the collection is one Lua-visible reference.

## Rules this enforces
- No dangling reference: tree lives as long as any Document/Element/Elements
  userdata referencing it is reachable from Lua.
- No double free: `lexbor_document_destroy` is called exactly once, gated by
  the refcount reaching zero, never called directly from any `__gc`.
- No raw pointer exposure: Lua only ever sees userdata; `handle` and `node`
  are never pushed as light userdata or convertible values.

## Test to add (Phase 4/6)
1. `local doc = html.parse(big_html); local t = doc:select("title")`
2. `doc = nil; collectgarbage()` — tree must **not** be freed yet.
3. `t:text()` — must still return correct text.
4. `t = nil; collectgarbage()` — tree freed now (verify via ASan/leak check).

This test should be part of Phase 4's "Force GC / Check memory" step, not
deferred to Phase 6, since it validates the ownership model the whole HTML
module depends on.

