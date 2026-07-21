#include "encoding_module.h"
#include "lualib.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// ─── Base64 ───────────────────────────────────────────────────────────────────

static const char B64_TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const int8_t B64_DECODE[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
    52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
    -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};

static int enc_base64_encode(lua_State *L) {
    size_t len;
    const uint8_t *src = (const uint8_t*)luaL_checklstring(L, 1, &len);
    size_t out_len = ((len + 2) / 3) * 4 + 1;
    char *out = (char*)malloc(out_len);
    if (!out) { luaL_error(L, "base64_encode: allocation failure"); return 0; }

    size_t oi = 0;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t b = ((uint32_t)src[i]) << 16;
        if (i + 1 < len) b |= ((uint32_t)src[i + 1]) << 8;
        if (i + 2 < len) b |= src[i + 2];
        out[oi++] = B64_TABLE[(b >> 18) & 0x3F];
        out[oi++] = B64_TABLE[(b >> 12) & 0x3F];
        out[oi++] = (i + 1 < len) ? B64_TABLE[(b >> 6) & 0x3F] : '=';
        out[oi++] = (i + 2 < len) ? B64_TABLE[b & 0x3F] : '=';
    }
    lua_pushlstring(L, out, oi);
    free(out);
    return 1;
}

static int enc_base64_decode(lua_State *L) {
    size_t len;
    const char *src = luaL_checklstring(L, 1, &len);
    size_t out_len = (len / 4) * 3 + 4;
    uint8_t *out = (uint8_t*)malloc(out_len);
    if (!out) { luaL_error(L, "base64_decode: allocation failure"); return 0; }

    size_t oi = 0;
    for (size_t i = 0; i + 3 < len; i += 4) {
        int8_t a = B64_DECODE[(uint8_t)src[i]];
        int8_t b = B64_DECODE[(uint8_t)src[i + 1]];
        int8_t c = B64_DECODE[(uint8_t)src[i + 2]];
        int8_t d = B64_DECODE[(uint8_t)src[i + 3]];
        if (a < 0 || b < 0) break;
        out[oi++] = (uint8_t)((a << 2) | (b >> 4));
        if (src[i + 2] != '=') out[oi++] = (uint8_t)((b << 4) | (c >> 2));
        if (src[i + 3] != '=') out[oi++] = (uint8_t)((c << 6) | d);
    }
    lua_pushlstring(L, (const char*)out, oi);
    free(out);
    return 1;
}

// ─── UTF-8 validation ─────────────────────────────────────────────────────────

static bool utf8_valid_codepoint(uint32_t cp) {
    // Reject surrogates, overlong values, and > U+10FFFF
    if (cp > 0x10FFFF) return false;
    if (cp >= 0xD800 && cp <= 0xDFFF) return false;
    return true;
}

static int enc_utf8_valid(lua_State *L) {
    size_t len;
    const uint8_t *s = (const uint8_t*)luaL_checklstring(L, 1, &len);
    size_t i = 0;
    while (i < len) {
        uint8_t b = s[i];
        uint32_t cp;
        int extra;
        if      (b < 0x80) { cp = b; extra = 0; }
        else if ((b & 0xE0) == 0xC0) { cp = b & 0x1F; extra = 1; }
        else if ((b & 0xF0) == 0xE0) { cp = b & 0x0F; extra = 2; }
        else if ((b & 0xF8) == 0xF0) { cp = b & 0x07; extra = 3; }
        else { lua_pushboolean(L, 0); return 1; }
        i++;
        for (int j = 0; j < extra; j++, i++) {
            if (i >= len || (s[i] & 0xC0) != 0x80) { lua_pushboolean(L, 0); return 1; }
            cp = (cp << 6) | (s[i] & 0x3F);
        }
        if (!utf8_valid_codepoint(cp)) { lua_pushboolean(L, 0); return 1; }
    }
    lua_pushboolean(L, 1);
    return 1;
}

// ─── UTF-8 ↔ UTF-16LE conversion ─────────────────────────────────────────────

// Encode a single codepoint to UTF-8, returns bytes written (1-4), 0 on error
static int cp_to_utf8(uint32_t cp, uint8_t *out) {
    if (!utf8_valid_codepoint(cp)) return 0;
    if (cp < 0x80)   { out[0] = (uint8_t)cp; return 1; }
    if (cp < 0x800)  { out[0]=0xC0|(cp>>6); out[1]=0x80|(cp&0x3F); return 2; }
    if (cp < 0x10000){ out[0]=0xE0|(cp>>12); out[1]=0x80|((cp>>6)&0x3F); out[2]=0x80|(cp&0x3F); return 3; }
    out[0]=0xF0|(cp>>18); out[1]=0x80|((cp>>12)&0x3F);
    out[2]=0x80|((cp>>6)&0x3F); out[3]=0x80|(cp&0x3F); return 4;
}

static int enc_utf8_to_utf16le(lua_State *L) {
    size_t len;
    const uint8_t *s = (const uint8_t*)luaL_checklstring(L, 1, &len);
    // Worst case: 4 bytes UTF-8 → 4 bytes UTF-16 (surrogate pair)
    uint8_t *out = (uint8_t*)malloc(len * 2 + 4);
    if (!out) { luaL_error(L, "utf8_to_utf16le: allocation failure"); return 0; }

    size_t i = 0, oi = 0;
    while (i < len) {
        uint8_t b = s[i];
        uint32_t cp;
        int extra;
        if      (b < 0x80) { cp = b; extra = 0; }
        else if ((b & 0xE0) == 0xC0) { cp = b & 0x1F; extra = 1; }
        else if ((b & 0xF0) == 0xE0) { cp = b & 0x0F; extra = 2; }
        else if ((b & 0xF8) == 0xF0) { cp = b & 0x07; extra = 3; }
        else { free(out); luaL_error(L, "utf8_to_utf16le: invalid UTF-8 byte at %d", (int)i); return 0; }
        i++;
        for (int j = 0; j < extra; j++, i++) {
            if (i >= len || (s[i] & 0xC0) != 0x80) { free(out); luaL_error(L, "utf8_to_utf16le: incomplete sequence"); return 0; }
            cp = (cp << 6) | (s[i] & 0x3F);
        }
        if (cp < 0x10000) {
            out[oi++] = cp & 0xFF;
            out[oi++] = (cp >> 8) & 0xFF;
        } else {
            // Surrogate pair
            cp -= 0x10000;
            uint16_t hi = 0xD800 | (cp >> 10);
            uint16_t lo = 0xDC00 | (cp & 0x3FF);
            out[oi++] = hi & 0xFF; out[oi++] = (hi >> 8) & 0xFF;
            out[oi++] = lo & 0xFF; out[oi++] = (lo >> 8) & 0xFF;
        }
    }
    lua_pushlstring(L, (const char*)out, oi);
    free(out);
    return 1;
}

static int enc_utf16le_to_utf8(lua_State *L) {
    size_t len;
    const uint8_t *s = (const uint8_t*)luaL_checklstring(L, 1, &len);
    // Worst case: 2 bytes UTF-16 → 3 bytes UTF-8
    uint8_t *out = (uint8_t*)malloc(len * 3 / 2 + 4);
    if (!out) { luaL_error(L, "utf16le_to_utf8: allocation failure"); return 0; }

    size_t i = 0, oi = 0;
    while (i + 1 < len) {
        uint16_t unit = (uint16_t)(s[i] | (s[i + 1] << 8));
        i += 2;
        uint32_t cp;
        if (unit >= 0xD800 && unit <= 0xDBFF) {
            // High surrogate — need low surrogate
            if (i + 1 >= len) { free(out); luaL_error(L, "utf16le_to_utf8: missing low surrogate"); return 0; }
            uint16_t lo = (uint16_t)(s[i] | (s[i + 1] << 8));
            i += 2;
            if (lo < 0xDC00 || lo > 0xDFFF) { free(out); luaL_error(L, "utf16le_to_utf8: invalid low surrogate"); return 0; }
            cp = 0x10000 + (((uint32_t)(unit - 0xD800)) << 10) + (lo - 0xDC00);
        } else {
            cp = unit;
        }
        uint8_t buf[4];
        int n = cp_to_utf8(cp, buf);
        for (int j = 0; j < n; j++) out[oi++] = buf[j];
    }
    lua_pushlstring(L, (const char*)out, oi);
    free(out);
    return 1;
}

// ─── Encoding detection ───────────────────────────────────────────────────────

static int enc_detect(lua_State *L) {
    size_t len;
    const uint8_t *s = (const uint8_t*)luaL_checklstring(L, 1, &len);

    // Check BOM
    if (len >= 2 && s[0] == 0xFF && s[1] == 0xFE) { lua_pushstring(L, "utf-16le"); return 1; }
    if (len >= 2 && s[0] == 0xFE && s[1] == 0xFF) { lua_pushstring(L, "utf-16be"); return 1; }
    if (len >= 3 && s[0] == 0xEF && s[1] == 0xBB && s[2] == 0xBF) { lua_pushstring(L, "utf-8-bom"); return 1; }
    if (len >= 4 && s[0] == 0x00 && s[1] == 0x00 && s[2] == 0xFE && s[3] == 0xFF) { lua_pushstring(L, "utf-32be"); return 1; }
    if (len >= 4 && s[0] == 0xFF && s[1] == 0xFE && s[2] == 0x00 && s[3] == 0x00) { lua_pushstring(L, "utf-32le"); return 1; }

    // Heuristic: check if valid UTF-8
    size_t i = 0;
    bool has_multibyte = false;
    bool valid_utf8 = true;
    while (i < len && valid_utf8) {
        uint8_t b = s[i];
        if (b < 0x80) { i++; continue; }
        has_multibyte = true;
        int extra;
        uint32_t cp;
        if      ((b & 0xE0) == 0xC0) { cp = b & 0x1F; extra = 1; }
        else if ((b & 0xF0) == 0xE0) { cp = b & 0x0F; extra = 2; }
        else if ((b & 0xF8) == 0xF0) { cp = b & 0x07; extra = 3; }
        else { valid_utf8 = false; break; }
        i++;
        for (int j = 0; j < extra; j++, i++) {
            if (i >= len || (s[i] & 0xC0) != 0x80) { valid_utf8 = false; break; }
            cp = (cp << 6) | (s[i] & 0x3F);
        }
        if (!utf8_valid_codepoint(cp)) valid_utf8 = false;
    }

    if (valid_utf8 && has_multibyte) { lua_pushstring(L, "utf-8"); return 1; }
    if (valid_utf8 && !has_multibyte) { lua_pushstring(L, "ascii"); return 1; }
    lua_pushstring(L, "latin-1");
    return 1;
}

// ─── String length (codepoint count) ─────────────────────────────────────────

static int enc_utf8_len(lua_State *L) {
    size_t byte_len;
    const uint8_t *s = (const uint8_t*)luaL_checklstring(L, 1, &byte_len);
    size_t count = 0;
    for (size_t i = 0; i < byte_len; i++) {
        // Count only leading bytes (not continuation bytes 0x80..0xBF)
        if ((s[i] & 0xC0) != 0x80) count++;
    }
    lua_pushinteger(L, (lua_Integer)count);
    return 1;
}

// ─── String byte slice (by codepoint index) ──────────────────────────────────

static int enc_utf8_sub(lua_State *L) {
    size_t byte_len;
    const uint8_t *s = (const uint8_t*)luaL_checklstring(L, 1, &byte_len);
    lua_Integer start = luaL_checkinteger(L, 2); // 1-based
    lua_Integer stop  = luaL_optinteger(L, 3, -1);

    // Count total codepoints to handle negative indices
    lua_Integer total = 0;
    for (size_t i = 0; i < byte_len; i++)
        if ((s[i] & 0xC0) != 0x80) total++;

    if (start < 0) start = total + start + 1;
    if (stop  < 0) stop  = total + stop  + 1;
    if (start < 1) start = 1;
    if (stop > total) stop = total;

    // Find byte offsets
    lua_Integer cp_idx = 0;
    size_t byte_start = 0, byte_end = byte_len;
    bool found_start = false;
    for (size_t i = 0; i <= byte_len; i++) {
        if (i == byte_len || (s[i] & 0xC0) != 0x80) {
            cp_idx++;
            if (cp_idx == start && !found_start) { byte_start = i; found_start = true; }
            if (cp_idx == stop + 1)              { byte_end = i; break; }
        }
    }
    if (!found_start || start > stop) { lua_pushstring(L, ""); return 1; }
    lua_pushlstring(L, (const char*)(s + byte_start), byte_end - byte_start);
    return 1;
}

// ─── Module open ─────────────────────────────────────────────────────────────

int luaopen_encoding(lua_State *L) {
    lua_newtable(L);

    lua_pushcfunction(L, enc_base64_encode,    "base64_encode");   lua_setfield(L, -2, "base64_encode");
    lua_pushcfunction(L, enc_base64_decode,    "base64_decode");   lua_setfield(L, -2, "base64_decode");
    lua_pushcfunction(L, enc_utf8_valid,       "utf8_valid");      lua_setfield(L, -2, "utf8_valid");
    lua_pushcfunction(L, enc_utf8_len,         "utf8_len");        lua_setfield(L, -2, "utf8_len");
    lua_pushcfunction(L, enc_utf8_sub,         "utf8_sub");        lua_setfield(L, -2, "utf8_sub");
    lua_pushcfunction(L, enc_utf8_to_utf16le,  "utf8_to_utf16le"); lua_setfield(L, -2, "utf8_to_utf16le");
    lua_pushcfunction(L, enc_utf16le_to_utf8,  "utf16le_to_utf8"); lua_setfield(L, -2, "utf16le_to_utf8");
    lua_pushcfunction(L, enc_detect,           "detect");          lua_setfield(L, -2, "detect");

    return 1;
}
