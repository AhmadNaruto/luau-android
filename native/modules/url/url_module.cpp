#include "url_module.h"
#include "lualib.h"
#include "lexbor/url/url.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// ─── Serialize callback accumulator ──────────────────────────────────────────

typedef struct {
    char   *buf;
    size_t  len;
    size_t  cap;
} url_str_t;

static lxb_status_t url_serialize_cb(const lxb_char_t *data, size_t len, void *ctx) {
    url_str_t *s = (url_str_t*)ctx;
    size_t needed = s->len + len + 1;
    if (needed > s->cap) {
        size_t new_cap = needed * 2;
        char *new_buf = (char*)realloc(s->buf, new_cap);
        if (!new_buf) return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
        s->buf = new_buf;
        s->cap = new_cap;
    }
    memcpy(s->buf + s->len, data, len);
    s->len += len;
    s->buf[s->len] = '\0';
    return LXB_STATUS_OK;
}

static void push_serialized(lua_State *L,
                             lxb_status_t (*fn)(const lxb_url_t*, lexbor_serialize_cb_f, void*),
                             const lxb_url_t *url) {
    url_str_t s = {NULL, 0, 0};
    if (fn(url, url_serialize_cb, &s) == LXB_STATUS_OK && s.len > 0) {
        lua_pushlstring(L, s.buf, s.len);
    } else {
        lua_pushstring(L, "");
    }
    if (s.buf) free(s.buf);
}

static void push_serialized_host(lua_State *L, const lxb_url_host_t *host) {
    url_str_t s = {NULL, 0, 0};
    if (lxb_url_serialize_host(host, url_serialize_cb, &s) == LXB_STATUS_OK && s.len > 0) {
        lua_pushlstring(L, s.buf, s.len);
    } else {
        lua_pushstring(L, "");
    }
    if (s.buf) free(s.buf);
}

static void push_serialized_path(lua_State *L, const lxb_url_path_t *path) {
    url_str_t s = {NULL, 0, 0};
    if (lxb_url_serialize_path(path, url_serialize_cb, &s) == LXB_STATUS_OK && s.len > 0) {
        lua_pushlstring(L, s.buf, s.len);
    } else {
        lua_pushstring(L, "");
    }
    if (s.buf) free(s.buf);
}

// ─── url.parse() ─────────────────────────────────────────────────────────────

static int url_parse(lua_State *L) {
    const char *url_str = luaL_checkstring(L, 1);

    lxb_url_parser_t *parser = lxb_url_parser_create();
    if (!parser) {
        luaL_error(L, "Failed to create URL parser");
        return 0;
    }

    lxb_status_t status = lxb_url_parser_init(parser, NULL);
    if (status != LXB_STATUS_OK) {
        lxb_url_parser_destroy(parser, true);
        luaL_error(L, "Failed to init URL parser");
        return 0;
    }

    lxb_url_t *url = lxb_url_parse(parser,
                                    NULL,
                                    (const lxb_char_t*)url_str, strlen(url_str));
    if (!url) {
        lxb_url_parser_destroy(parser, true);
        luaL_error(L, "Failed to parse URL: %s", url_str);
        return 0;
    }

    lua_newtable(L);

    // scheme
    push_serialized(L, lxb_url_serialize_scheme, url);
    lua_setfield(L, -2, "scheme");

    // host
    push_serialized_host(L, &url->host);
    lua_setfield(L, -2, "host");

    // port
    if (url->has_port) {
        lua_pushinteger(L, url->port);
    } else {
        lua_pushnil(L);
    }
    lua_setfield(L, -2, "port");

    // path
    push_serialized_path(L, &url->path);
    lua_setfield(L, -2, "path");

    // query
    if (url->query.length > 0) {
        push_serialized(L, lxb_url_serialize_query, url);
    } else {
        lua_pushstring(L, "");
    }
    lua_setfield(L, -2, "query");

    // fragment
    if (url->fragment.length > 0) {
        push_serialized(L, lxb_url_serialize_fragment, url);
    } else {
        lua_pushstring(L, "");
    }
    lua_setfield(L, -2, "fragment");

    // href (full serialized URL)
    {
        url_str_t s = {NULL, 0, 0};
        lxb_url_serialize(url, url_serialize_cb, &s, false);
        if (s.len > 0) {
            lua_pushlstring(L, s.buf, s.len);
        } else {
            lua_pushstring(L, "");
        }
        if (s.buf) free(s.buf);
    }
    lua_setfield(L, -2, "href");

    lxb_url_parser_destroy(parser, true);
    return 1;
}

// ─── url.join() ──────────────────────────────────────────────────────────────

static int url_join(lua_State *L) {
    const char *base_str = luaL_checkstring(L, 1);
    const char *rel_str  = luaL_checkstring(L, 2);

    lxb_url_parser_t *parser = lxb_url_parser_create();
    if (!parser) { luaL_error(L, "Failed to create URL parser"); return 0; }
    if (lxb_url_parser_init(parser, NULL) != LXB_STATUS_OK) {
        lxb_url_parser_destroy(parser, true);
        luaL_error(L, "Failed to init URL parser");
        return 0;
    }

    // Parse base first
    lxb_url_t *base = lxb_url_parse(parser, NULL,
                                     (const lxb_char_t*)base_str, strlen(base_str));
    if (!base) {
        lxb_url_parser_destroy(parser, true);
        luaL_error(L, "Failed to parse base URL: %s", base_str);
        return 0;
    }

    // Re-init for relative parsing but keep mraw from base URL alive
    lxb_url_parser_clean(parser);

    lxb_url_t *resolved = lxb_url_parse(parser, base,
                                          (const lxb_char_t*)rel_str, strlen(rel_str));
    if (!resolved) {
        lxb_url_parser_destroy(parser, true);
        luaL_error(L, "Failed to resolve URL: %s against %s", rel_str, base_str);
        return 0;
    }

    url_str_t s = {NULL, 0, 0};
    lxb_url_serialize(resolved, url_serialize_cb, &s, false);
    if (s.len > 0) {
        lua_pushlstring(L, s.buf, s.len);
    } else {
        lua_pushstring(L, "");
    }
    if (s.buf) free(s.buf);

    lxb_url_parser_destroy(parser, true);
    return 1;
}

// ─── url.encode() / url.decode() ─────────────────────────────────────────────

static const char *HEX = "0123456789ABCDEF";

static bool is_unreserved(unsigned char c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.' || c == '~';
}

static int url_encode(lua_State *L) {
    size_t len = 0;
    const char *src = luaL_checklstring(L, 1, &len);

    // Worst case: every byte becomes %XX
    char *out = (char*)malloc(len * 3 + 1);
    if (!out) { luaL_error(L, "url.encode allocation failure"); return 0; }

    size_t oi = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)src[i];
        if (is_unreserved(c)) {
            out[oi++] = c;
        } else {
            out[oi++] = '%';
            out[oi++] = HEX[c >> 4];
            out[oi++] = HEX[c & 0xF];
        }
    }
    out[oi] = '\0';
    lua_pushlstring(L, out, oi);
    free(out);
    return 1;
}

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static int url_decode(lua_State *L) {
    size_t len = 0;
    const char *src = luaL_checklstring(L, 1, &len);

    char *out = (char*)malloc(len + 1);
    if (!out) { luaL_error(L, "url.decode allocation failure"); return 0; }

    size_t oi = 0;
    for (size_t i = 0; i < len; i++) {
        if (src[i] == '%' && i + 2 < len) {
            int hi = hex_digit(src[i + 1]);
            int lo = hex_digit(src[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out[oi++] = (char)((hi << 4) | lo);
                i += 2;
                continue;
            }
        } else if (src[i] == '+') {
            out[oi++] = ' ';
            continue;
        }
        out[oi++] = src[i];
    }
    out[oi] = '\0';
    lua_pushlstring(L, out, oi);
    free(out);
    return 1;
}

// ─── Module open ─────────────────────────────────────────────────────────────

int luaopen_url(lua_State *L) {
    lua_newtable(L);

    lua_pushcfunction(L, url_parse, "parse");
    lua_setfield(L, -2, "parse");

    lua_pushcfunction(L, url_join, "join");
    lua_setfield(L, -2, "join");

    lua_pushcfunction(L, url_encode, "encode");
    lua_setfield(L, -2, "encode");

    lua_pushcfunction(L, url_decode, "decode");
    lua_setfield(L, -2, "decode");

    return 1;
}
