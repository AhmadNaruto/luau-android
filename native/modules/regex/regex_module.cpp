#define PCRE2_CODE_UNIT_WIDTH 8
#include "regex_module.h"
#include "lualib.h"
#include "pcre2.h"
#include <stdlib.h>
#include <string.h>

// ─── Userdata ─────────────────────────────────────────────────────────────────

typedef struct {
    pcre2_code *code;
} regex_t;

// ─── regex.compile(pattern [, flags]) ────────────────────────────────────────

static int regex_compile(lua_State *L) {
    const char *pattern = luaL_checkstring(L, 1);
    uint32_t options = 0;

    // Optional second argument: flag string, e.g. "i", "im", "ix"
    if (!lua_isnoneornil(L, 2)) {
        const char *flags = luaL_checkstring(L, 2);
        for (const char *f = flags; *f; f++) {
            switch (*f) {
                case 'i': options |= PCRE2_CASELESS;  break;
                case 'm': options |= PCRE2_MULTILINE; break;
                case 's': options |= PCRE2_DOTALL;    break;
                case 'x': options |= PCRE2_EXTENDED;  break;
                default: break;
            }
        }
    }

    int errornumber;
    PCRE2_SIZE erroroffset;
    pcre2_code *code = pcre2_compile(
        (PCRE2_SPTR)pattern,
        PCRE2_ZERO_TERMINATED,
        options,
        &errornumber,
        &erroroffset,
        NULL
    );

    if (!code) {
        PCRE2_UCHAR error_buf[256];
        pcre2_get_error_message(errornumber, error_buf, sizeof(error_buf));
        luaL_error(L, "Failed to compile regex pattern: %s (at offset %d)", (char*)error_buf, (int)erroroffset);
        return 0;
    }

    regex_t *re = (regex_t*)lua_newuserdata(L, sizeof(regex_t));
    re->code = code;

    luaL_getmetatable(L, "LuauRegex");
    lua_setmetatable(L, -2);
    return 1;
}

// ─── __gc ─────────────────────────────────────────────────────────────────────

static int regex_gc(lua_State *L) {
    regex_t *re = (regex_t*)lua_touserdata(L, 1);
    if (re && re->code) {
        pcre2_code_free(re->code);
        re->code = NULL;
    }
    return 0;
}

// ─── Internal: run one match, push result table or nil ───────────────────────

// Returns rc on match (≥1), PCRE2_ERROR_NOMATCH or other negative on no-match/error.
static int run_match(lua_State *L, pcre2_code *code,
                     const char *subject, size_t slen, PCRE2_SIZE offset,
                     pcre2_match_data **out_md) {
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(code, NULL);
    if (!md) { luaL_error(L, "Failed to create PCRE2 match data"); return 0; }

    int rc = pcre2_match(code, (PCRE2_SPTR)subject, slen, offset, 0, md, NULL);
    if (rc < 0) {
        pcre2_match_data_free(md);
        *out_md = NULL;
        return rc;
    }
    *out_md = md;
    return rc;
}

// Push a match result table onto the Lua stack.
// Needs access to code for named captures.
static void push_match_result(lua_State *L, pcre2_code *code,
                               const char *subject, pcre2_match_data *md, int rc) {
    lua_newtable(L);
    PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(md);

    for (int i = 0; i < rc; i++) {
        PCRE2_SIZE start = ovector[2 * i];
        PCRE2_SIZE end   = ovector[2 * i + 1];
        if (start != PCRE2_UNSET && end != PCRE2_UNSET) {
            lua_pushlstring(L, subject + start, end - start);
        } else {
            lua_pushnil(L);
        }
        lua_rawseti(L, -2, i);
    }

    // Named captures
    uint32_t name_count = 0;
    pcre2_pattern_info(code, PCRE2_INFO_NAMECOUNT, &name_count);
    if (name_count > 0) {
        PCRE2_SPTR name_table = NULL;
        uint32_t name_entry_size = 0;
        pcre2_pattern_info(code, PCRE2_INFO_NAMETABLE,    &name_table);
        pcre2_pattern_info(code, PCRE2_INFO_NAMEENTRYSIZE, &name_entry_size);

        PCRE2_SPTR tabptr = name_table;
        for (uint32_t i = 0; i < name_count; i++) {
            uint32_t group_index = ((uint32_t)tabptr[0] << 8) | tabptr[1];
            const char *name = (const char*)(tabptr + 2);

            if (group_index < (uint32_t)rc) {
                PCRE2_SIZE start = ovector[2 * group_index];
                PCRE2_SIZE end   = ovector[2 * group_index + 1];
                if (start != PCRE2_UNSET && end != PCRE2_UNSET) {
                    lua_pushlstring(L, subject + start, end - start);
                } else {
                    lua_pushnil(L);
                }
                lua_setfield(L, -2, name);
            }
            tabptr += name_entry_size;
        }
    }
}

// ─── re:match(subject) → table | nil ─────────────────────────────────────────

static int regex_match(lua_State *L) {
    regex_t *re = (regex_t*)luaL_checkudata(L, 1, "LuauRegex");
    size_t slen = 0;
    const char *subject = luaL_checklstring(L, 2, &slen);

    pcre2_match_data *md = NULL;
    int rc = run_match(L, re->code, subject, slen, 0, &md);
    if (rc < 0) {
        lua_pushnil(L);
        return 1;
    }

    push_match_result(L, re->code, subject, md, rc);
    pcre2_match_data_free(md);
    return 1;
}

// ─── re:find(subject [, offset]) → table | nil ───────────────────────────────
// Same as match but accepts a starting byte offset (0-based).

static int regex_find(lua_State *L) {
    regex_t *re = (regex_t*)luaL_checkudata(L, 1, "LuauRegex");
    size_t slen = 0;
    const char *subject = luaL_checklstring(L, 2, &slen);
    PCRE2_SIZE offset = (PCRE2_SIZE)luaL_optinteger(L, 3, 0);

    if (offset > slen) {
        lua_pushnil(L);
        return 1;
    }

    pcre2_match_data *md = NULL;
    int rc = run_match(L, re->code, subject, slen, offset, &md);
    if (rc < 0) {
        lua_pushnil(L);
        return 1;
    }

    push_match_result(L, re->code, subject, md, rc);
    pcre2_match_data_free(md);
    return 1;
}

// ─── re:find_all(subject) → array of match tables ────────────────────────────

static int regex_find_all(lua_State *L) {
    regex_t *re = (regex_t*)luaL_checkudata(L, 1, "LuauRegex");
    size_t slen = 0;
    const char *subject = luaL_checklstring(L, 2, &slen);

    lua_newtable(L); // result array
    int result_idx = lua_gettop(L);
    int count = 0;

    PCRE2_SIZE offset = 0;
    while (offset <= slen) {
        pcre2_match_data *md = NULL;
        int rc = run_match(L, re->code, subject, slen, offset, &md);
        if (rc < 0) break;

        push_match_result(L, re->code, subject, md, rc);
        lua_rawseti(L, result_idx, ++count);

        PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);
        PCRE2_SIZE next = ov[1];
        pcre2_match_data_free(md);

        // Advance at least one byte to avoid infinite loop on zero-length match
        if (next == offset) next = offset + 1;
        offset = next;
    }

    return 1;
}

// ─── re:replace(subject, replacement [, all]) → string ───────────────────────
// replacement is a plain string; $0..$9 and ${name} substitutions supported.

static void append_buf(char **buf, size_t *len, size_t *cap,
                       const char *data, size_t dlen) {
    size_t needed = *len + dlen + 1;
    if (needed > *cap) {
        *cap = needed * 2 > 64 ? needed * 2 : 64;
        *buf = (char*)realloc(*buf, *cap);
    }
    memcpy(*buf + *len, data, dlen);
    *len += dlen;
    (*buf)[*len] = '\0';
}

// Expand a replacement string with $0/$1/... and ${name} references.
static void expand_replacement(const char *repl, size_t repl_len,
                                const char *subject,
                                pcre2_code *code,
                                pcre2_match_data *md, int rc,
                                char **out, size_t *out_len, size_t *out_cap) {
    PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);

    for (size_t i = 0; i < repl_len; ) {
        if (repl[i] == '$' && i + 1 < repl_len) {
            i++;
            if (repl[i] == '{') {
                // ${name} or ${N}
                i++;
                size_t name_start = i;
                while (i < repl_len && repl[i] != '}') i++;
                size_t name_len = i - name_start;
                if (i < repl_len) i++; // skip '}'

                char name_buf[128];
                if (name_len < sizeof(name_buf)) {
                    memcpy(name_buf, repl + name_start, name_len);
                    name_buf[name_len] = '\0';
                }

                // Try numeric first
                char *endp = NULL;
                long idx = strtol(name_buf, &endp, 10);
                if (endp == name_buf + name_len) {
                    // It's a number
                    if (idx >= 0 && idx < rc) {
                        PCRE2_SIZE s = ov[2*idx], e = ov[2*idx+1];
                        if (s != PCRE2_UNSET && e != PCRE2_UNSET)
                            append_buf(out, out_len, out_cap, subject + s, e - s);
                    }
                } else {
                    // Named capture
                    int named_idx = pcre2_substring_number_from_name(
                        code, (PCRE2_SPTR)name_buf);
                    if (named_idx > 0 && named_idx < rc) {
                        PCRE2_SIZE s = ov[2*named_idx], e = ov[2*named_idx+1];
                        if (s != PCRE2_UNSET && e != PCRE2_UNSET)
                            append_buf(out, out_len, out_cap, subject + s, e - s);
                    }
                }
            } else if (repl[i] >= '0' && repl[i] <= '9') {
                int idx = repl[i++] - '0';
                if (idx < rc) {
                    PCRE2_SIZE s = ov[2*idx], e = ov[2*idx+1];
                    if (s != PCRE2_UNSET && e != PCRE2_UNSET)
                        append_buf(out, out_len, out_cap, subject + s, e - s);
                }
            } else {
                // Literal '$'
                append_buf(out, out_len, out_cap, "$", 1);
                append_buf(out, out_len, out_cap, repl + i, 1);
                i++;
            }
        } else {
            append_buf(out, out_len, out_cap, repl + i, 1);
            i++;
        }
    }
}

static int regex_replace(lua_State *L) {
    regex_t *re = (regex_t*)luaL_checkudata(L, 1, "LuauRegex");
    size_t slen = 0;
    const char *subject = luaL_checklstring(L, 2, &slen);
    size_t repl_len = 0;
    const char *repl = luaL_checklstring(L, 3, &repl_len);
    // 4th arg: replace_all (bool, default false)
    bool replace_all = !lua_isnoneornil(L, 4) && lua_toboolean(L, 4);

    char *out = NULL;
    size_t out_len = 0, out_cap = 0;

    PCRE2_SIZE offset = 0;
    bool replaced = false;

    while (offset <= slen) {
        pcre2_match_data *md = NULL;
        int rc = run_match(L, re->code, subject, slen, offset, &md);
        if (rc < 0) {
            // No more matches
            append_buf(&out, &out_len, &out_cap, subject + offset, slen - offset);
            break;
        }

        PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);
        PCRE2_SIZE match_start = ov[0];
        PCRE2_SIZE match_end   = ov[1];

        // Append text before this match
        append_buf(&out, &out_len, &out_cap, subject + offset, match_start - offset);

        // Append expanded replacement
        expand_replacement(repl, repl_len, subject, re->code, md, rc,
                           &out, &out_len, &out_cap);

        pcre2_match_data_free(md);
        replaced = true;

        PCRE2_SIZE next = match_end;
        if (next == offset) {
            // Zero-length match: copy one char and advance
            if (offset < slen)
                append_buf(&out, &out_len, &out_cap, subject + offset, 1);
            next = offset + 1;
        }
        offset = next;

        if (!replace_all) {
            // Append rest of subject and stop
            append_buf(&out, &out_len, &out_cap, subject + offset, slen - offset);
            break;
        }
    }

    (void)replaced;
    if (!out) {
        lua_pushlstring(L, subject, slen);
    } else {
        lua_pushlstring(L, out, out_len);
        free(out);
    }
    return 1;
}

// ─── re:split(subject [, limit]) → array of strings ──────────────────────────

static int regex_split(lua_State *L) {
    regex_t *re = (regex_t*)luaL_checkudata(L, 1, "LuauRegex");
    size_t slen = 0;
    const char *subject = luaL_checklstring(L, 2, &slen);
    int limit = (int)luaL_optinteger(L, 3, 0); // 0 = no limit

    lua_newtable(L);
    int result_idx = lua_gettop(L);
    int count = 0;

    PCRE2_SIZE offset = 0;
    while (offset <= slen) {
        if (limit > 0 && count >= limit - 1) {
            // Push remaining as last element
            lua_pushlstring(L, subject + offset, slen - offset);
            lua_rawseti(L, result_idx, ++count);
            break;
        }

        pcre2_match_data *md = NULL;
        int rc = run_match(L, re->code, subject, slen, offset, &md);
        if (rc < 0) {
            // No more matches — push remainder
            lua_pushlstring(L, subject + offset, slen - offset);
            lua_rawseti(L, result_idx, ++count);
            break;
        }

        PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);
        PCRE2_SIZE match_start = ov[0];
        PCRE2_SIZE match_end   = ov[1];

        // Push segment before this match
        lua_pushlstring(L, subject + offset, match_start - offset);
        lua_rawseti(L, result_idx, ++count);

        pcre2_match_data_free(md);

        PCRE2_SIZE next = match_end;
        if (next == offset) next = offset + 1; // avoid infinite loop
        offset = next;
    }

    return 1;
}

// ─── Module open ─────────────────────────────────────────────────────────────

int luaopen_regex(lua_State *L) {
    // Create metatable for LuauRegex
    luaL_newmetatable(L, "LuauRegex");

    lua_pushcfunction(L, regex_gc, "__gc");
    lua_setfield(L, -2, "__gc");

    lua_newtable(L); // __index table
    lua_pushcfunction(L, regex_match,    "match");    lua_setfield(L, -2, "match");
    lua_pushcfunction(L, regex_find,     "find");     lua_setfield(L, -2, "find");
    lua_pushcfunction(L, regex_find_all, "find_all"); lua_setfield(L, -2, "find_all");
    lua_pushcfunction(L, regex_replace,  "replace");  lua_setfield(L, -2, "replace");
    lua_pushcfunction(L, regex_split,    "split");    lua_setfield(L, -2, "split");
    lua_setfield(L, -2, "__index");

    lua_pop(L, 1); // pop metatable

    // Module table
    lua_newtable(L);
    lua_pushcfunction(L, regex_compile, "compile");
    lua_setfield(L, -2, "compile");

    return 1;
}
