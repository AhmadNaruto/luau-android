#include "buffer_module.h"
#include "lualib.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Buffer Userdata structure
typedef struct {
    uint8_t *data;
    size_t size;
    bool own;
} sdk_buffer_t;

// Bounds checking helper
static void check_bounds(lua_State *L, sdk_buffer_t *buf, size_t offset, size_t len) {
    if (offset + len > buf->size) {
        luaL_error(L, "Out of bounds buffer access: offset %d, len %d, size %d", (int)offset, (int)len, (int)buf->size);
    }
}

static int buffer_create(lua_State *L) {
    int size = luaL_checkinteger(L, 1);
    if (size < 0) {
        luaL_error(L, "Buffer size cannot be negative: %d", size);
        return 0;
    }

    sdk_buffer_t *buf = (sdk_buffer_t*)lua_newuserdata(L, sizeof(sdk_buffer_t));
    buf->size = (size_t)size;
    buf->own = true;
    buf->data = (uint8_t*)calloc(1, buf->size);
    if (size > 0 && !buf->data) {
        luaL_error(L, "Failed to allocate memory for buffer of size: %d", size);
        return 0;
    }

    luaL_getmetatable(L, "LuauSdkBuffer");
    lua_setmetatable(L, -2);
    return 1;
}

static int buffer_gc(lua_State *L) {
    sdk_buffer_t *buf = (sdk_buffer_t*)lua_touserdata(L, 1);
    if (buf && buf->own && buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
    return 0;
}

static int buffer_size(lua_State *L) {
    sdk_buffer_t *buf = (sdk_buffer_t*)luaL_checkudata(L, 1, "LuauSdkBuffer");
    lua_pushinteger(L, buf->size);
    return 1;
}

static int buffer_writeString(lua_State *L) {
    sdk_buffer_t *buf = (sdk_buffer_t*)luaL_checkudata(L, 1, "LuauSdkBuffer");
    int offset = luaL_checkinteger(L, 2);
    size_t len = 0;
    const char *str = luaL_checklstring(L, 3, &len);

    if (offset < 0) {
        luaL_error(L, "Offset cannot be negative: %d", offset);
        return 0;
    }

    check_bounds(L, buf, (size_t)offset, len);
    memcpy(buf->data + offset, str, len);
    return 0;
}

static int buffer_readString(lua_State *L) {
    sdk_buffer_t *buf = (sdk_buffer_t*)luaL_checkudata(L, 1, "LuauSdkBuffer");
    int offset = luaL_checkinteger(L, 2);
    int len = luaL_checkinteger(L, 3);

    if (offset < 0 || len < 0) {
        luaL_error(L, "Offset and length cannot be negative");
        return 0;
    }

    check_bounds(L, buf, (size_t)offset, (size_t)len);
    lua_pushlstring(L, (const char*)(buf->data + offset), (size_t)len);
    return 1;
}

static int buffer_writeUInt8(lua_State *L) {
    sdk_buffer_t *buf = (sdk_buffer_t*)luaL_checkudata(L, 1, "LuauSdkBuffer");
    int offset = luaL_checkinteger(L, 2);
    int val = luaL_checkinteger(L, 3);
    if (offset < 0) return luaL_error(L, "Negative offset"), 0;
    check_bounds(L, buf, (size_t)offset, 1);
    buf->data[offset] = (uint8_t)val;
    return 0;
}

static int buffer_readUInt8(lua_State *L) {
    sdk_buffer_t *buf = (sdk_buffer_t*)luaL_checkudata(L, 1, "LuauSdkBuffer");
    int offset = luaL_checkinteger(L, 2);
    if (offset < 0) return luaL_error(L, "Negative offset"), 0;
    check_bounds(L, buf, (size_t)offset, 1);
    lua_pushinteger(L, buf->data[offset]);
    return 1;
}

static int buffer_writeInt8(lua_State *L) {
    sdk_buffer_t *buf = (sdk_buffer_t*)luaL_checkudata(L, 1, "LuauSdkBuffer");
    int offset = luaL_checkinteger(L, 2);
    int val = luaL_checkinteger(L, 3);
    if (offset < 0) return luaL_error(L, "Negative offset"), 0;
    check_bounds(L, buf, (size_t)offset, 1);
    buf->data[offset] = (uint8_t)(int8_t)val;
    return 0;
}

static int buffer_readInt8(lua_State *L) {
    sdk_buffer_t *buf = (sdk_buffer_t*)luaL_checkudata(L, 1, "LuauSdkBuffer");
    int offset = luaL_checkinteger(L, 2);
    if (offset < 0) return luaL_error(L, "Negative offset"), 0;
    check_bounds(L, buf, (size_t)offset, 1);
    lua_pushinteger(L, (int8_t)buf->data[offset]);
    return 1;
}

static int buffer_writeUInt16(lua_State *L) {
    sdk_buffer_t *buf = (sdk_buffer_t*)luaL_checkudata(L, 1, "LuauSdkBuffer");
    int offset = luaL_checkinteger(L, 2);
    int val = luaL_checkinteger(L, 3);
    if (offset < 0) return luaL_error(L, "Negative offset"), 0;
    check_bounds(L, buf, (size_t)offset, 2);
    uint16_t u16 = (uint16_t)val;
    memcpy(buf->data + offset, &u16, 2);
    return 0;
}

static int buffer_readUInt16(lua_State *L) {
    sdk_buffer_t *buf = (sdk_buffer_t*)luaL_checkudata(L, 1, "LuauSdkBuffer");
    int offset = luaL_checkinteger(L, 2);
    if (offset < 0) return luaL_error(L, "Negative offset"), 0;
    check_bounds(L, buf, (size_t)offset, 2);
    uint16_t u16;
    memcpy(&u16, buf->data + offset, 2);
    lua_pushinteger(L, u16);
    return 1;
}

static int buffer_writeInt16(lua_State *L) {
    sdk_buffer_t *buf = (sdk_buffer_t*)luaL_checkudata(L, 1, "LuauSdkBuffer");
    int offset = luaL_checkinteger(L, 2);
    int val = luaL_checkinteger(L, 3);
    if (offset < 0) return luaL_error(L, "Negative offset"), 0;
    check_bounds(L, buf, (size_t)offset, 2);
    int16_t s16 = (int16_t)val;
    memcpy(buf->data + offset, &s16, 2);
    return 0;
}

static int buffer_readInt16(lua_State *L) {
    sdk_buffer_t *buf = (sdk_buffer_t*)luaL_checkudata(L, 1, "LuauSdkBuffer");
    int offset = luaL_checkinteger(L, 2);
    if (offset < 0) return luaL_error(L, "Negative offset"), 0;
    check_bounds(L, buf, (size_t)offset, 2);
    int16_t s16;
    memcpy(&s16, buf->data + offset, 2);
    lua_pushinteger(L, s16);
    return 1;
}

static int buffer_writeUInt32(lua_State *L) {
    sdk_buffer_t *buf = (sdk_buffer_t*)luaL_checkudata(L, 1, "LuauSdkBuffer");
    int offset = luaL_checkinteger(L, 2);
    double val = luaL_checknumber(L, 3);
    if (offset < 0) return luaL_error(L, "Negative offset"), 0;
    check_bounds(L, buf, (size_t)offset, 4);
    uint32_t u32 = (uint32_t)val;
    memcpy(buf->data + offset, &u32, 4);
    return 0;
}

static int buffer_readUInt32(lua_State *L) {
    sdk_buffer_t *buf = (sdk_buffer_t*)luaL_checkudata(L, 1, "LuauSdkBuffer");
    int offset = luaL_checkinteger(L, 2);
    if (offset < 0) return luaL_error(L, "Negative offset"), 0;
    check_bounds(L, buf, (size_t)offset, 4);
    uint32_t u32;
    memcpy(&u32, buf->data + offset, 4);
    lua_pushnumber(L, u32);
    return 1;
}

static int buffer_writeInt32(lua_State *L) {
    sdk_buffer_t *buf = (sdk_buffer_t*)luaL_checkudata(L, 1, "LuauSdkBuffer");
    int offset = luaL_checkinteger(L, 2);
    int val = luaL_checkinteger(L, 3);
    if (offset < 0) return luaL_error(L, "Negative offset"), 0;
    check_bounds(L, buf, (size_t)offset, 4);
    int32_t s32 = (int32_t)val;
    memcpy(buf->data + offset, &s32, 4);
    return 0;
}

static int buffer_readInt32(lua_State *L) {
    sdk_buffer_t *buf = (sdk_buffer_t*)luaL_checkudata(L, 1, "LuauSdkBuffer");
    int offset = luaL_checkinteger(L, 2);
    if (offset < 0) return luaL_error(L, "Negative offset"), 0;
    check_bounds(L, buf, (size_t)offset, 4);
    int32_t s32;
    memcpy(&s32, buf->data + offset, 4);
    lua_pushinteger(L, s32);
    return 1;
}

static int buffer_writeFloat32(lua_State *L) {
    sdk_buffer_t *buf = (sdk_buffer_t*)luaL_checkudata(L, 1, "LuauSdkBuffer");
    int offset = luaL_checkinteger(L, 2);
    double val = luaL_checknumber(L, 3);
    if (offset < 0) return luaL_error(L, "Negative offset"), 0;
    check_bounds(L, buf, (size_t)offset, 4);
    float f32 = (float)val;
    memcpy(buf->data + offset, &f32, 4);
    return 0;
}

static int buffer_readFloat32(lua_State *L) {
    sdk_buffer_t *buf = (sdk_buffer_t*)luaL_checkudata(L, 1, "LuauSdkBuffer");
    int offset = luaL_checkinteger(L, 2);
    if (offset < 0) return luaL_error(L, "Negative offset"), 0;
    check_bounds(L, buf, (size_t)offset, 4);
    float f32;
    memcpy(&f32, buf->data + offset, 4);
    lua_pushnumber(L, f32);
    return 1;
}

static int buffer_writeFloat64(lua_State *L) {
    sdk_buffer_t *buf = (sdk_buffer_t*)luaL_checkudata(L, 1, "LuauSdkBuffer");
    int offset = luaL_checkinteger(L, 2);
    double val = luaL_checknumber(L, 3);
    if (offset < 0) return luaL_error(L, "Negative offset"), 0;
    check_bounds(L, buf, (size_t)offset, 8);
    memcpy(buf->data + offset, &val, 8);
    return 0;
}

static int buffer_readFloat64(lua_State *L) {
    sdk_buffer_t *buf = (sdk_buffer_t*)luaL_checkudata(L, 1, "LuauSdkBuffer");
    int offset = luaL_checkinteger(L, 2);
    if (offset < 0) return luaL_error(L, "Negative offset"), 0;
    check_bounds(L, buf, (size_t)offset, 8);
    double f64;
    memcpy(&f64, buf->data + offset, 8);
    lua_pushnumber(L, f64);
    return 1;
}

static int buffer_copy(lua_State *L) {
    sdk_buffer_t *src = (sdk_buffer_t*)luaL_checkudata(L, 1, "LuauSdkBuffer");
    sdk_buffer_t *target = (sdk_buffer_t*)luaL_checkudata(L, 2, "LuauSdkBuffer");
    int targetOffset = luaL_checkinteger(L, 3);
    int sourceOffset = luaL_checkinteger(L, 4);
    int len = luaL_checkinteger(L, 5);

    if (targetOffset < 0 || sourceOffset < 0 || len < 0) {
        return luaL_error(L, "Negative parameters not allowed"), 0;
    }

    check_bounds(L, src, (size_t)sourceOffset, (size_t)len);
    check_bounds(L, target, (size_t)targetOffset, (size_t)len);

    memcpy(target->data + targetOffset, src->data + sourceOffset, (size_t)len);
    return 0;
}

static int buffer_slice(lua_State *L) {
    sdk_buffer_t *buf = (sdk_buffer_t*)luaL_checkudata(L, 1, "LuauSdkBuffer");
    int offset = luaL_checkinteger(L, 2);
    int len = luaL_checkinteger(L, 3);

    if (offset < 0 || len < 0) {
        return luaL_error(L, "Negative parameters not allowed"), 0;
    }

    check_bounds(L, buf, (size_t)offset, (size_t)len);

    sdk_buffer_t *sliced = (sdk_buffer_t*)lua_newuserdata(L, sizeof(sdk_buffer_t));
    sliced->size = (size_t)len;
    sliced->own = true;
    sliced->data = (uint8_t*)malloc(sliced->size);
    if (len > 0 && !sliced->data) {
        return luaL_error(L, "Allocation failed"), 0;
    }

    memcpy(sliced->data, buf->data + offset, sliced->size);

    luaL_getmetatable(L, "LuauSdkBuffer");
    lua_setmetatable(L, -2);
    return 1;
}

int luaopen_custom_buffer(lua_State *L) {
    // 1. Create metatable for LuauSdkBuffer
    luaL_newmetatable(L, "LuauSdkBuffer");
    lua_pushcfunction(L, buffer_gc, "__gc");
    lua_setfield(L, -2, "__gc");

    // Methods
    lua_newtable(L);
    lua_pushcfunction(L, buffer_size, "size");
    lua_setfield(L, -2, "size");
    lua_pushcfunction(L, buffer_writeString, "writeString");
    lua_setfield(L, -2, "writeString");
    lua_pushcfunction(L, buffer_readString, "readString");
    lua_setfield(L, -2, "readString");
    lua_pushcfunction(L, buffer_writeUInt8, "writeUInt8");
    lua_setfield(L, -2, "writeUInt8");
    lua_pushcfunction(L, buffer_readUInt8, "readUInt8");
    lua_setfield(L, -2, "readUInt8");
    lua_pushcfunction(L, buffer_writeInt8, "writeInt8");
    lua_setfield(L, -2, "writeInt8");
    lua_pushcfunction(L, buffer_readInt8, "readInt8");
    lua_setfield(L, -2, "readInt8");
    
    lua_pushcfunction(L, buffer_writeUInt16, "writeUInt16");
    lua_setfield(L, -2, "writeUInt16");
    lua_pushcfunction(L, buffer_readUInt16, "readUInt16");
    lua_setfield(L, -2, "readUInt16");
    lua_pushcfunction(L, buffer_writeInt16, "writeInt16");
    lua_setfield(L, -2, "writeInt16");
    lua_pushcfunction(L, buffer_readInt16, "readInt16");
    lua_setfield(L, -2, "readInt16");

    lua_pushcfunction(L, buffer_writeUInt32, "writeUInt32");
    lua_setfield(L, -2, "writeUInt32");
    lua_pushcfunction(L, buffer_readUInt32, "readUInt32");
    lua_setfield(L, -2, "readUInt32");
    lua_pushcfunction(L, buffer_writeInt32, "writeInt32");
    lua_setfield(L, -2, "writeInt32");
    lua_pushcfunction(L, buffer_readInt32, "readInt32");
    lua_setfield(L, -2, "readInt32");

    lua_pushcfunction(L, buffer_writeFloat32, "writeFloat32");
    lua_setfield(L, -2, "writeFloat32");
    lua_pushcfunction(L, buffer_readFloat32, "readFloat32");
    lua_setfield(L, -2, "readFloat32");
    lua_pushcfunction(L, buffer_writeFloat64, "writeFloat64");
    lua_setfield(L, -2, "writeFloat64");
    lua_pushcfunction(L, buffer_readFloat64, "readFloat64");
    lua_setfield(L, -2, "readFloat64");

    lua_pushcfunction(L, buffer_copy, "copy");
    lua_setfield(L, -2, "copy");
    lua_pushcfunction(L, buffer_slice, "slice");
    lua_setfield(L, -2, "slice");

    // Set methods table to __index
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    // 2. Create module functions
    lua_newtable(L);
    lua_pushcfunction(L, buffer_create, "create");
    lua_setfield(L, -2, "create");

    return 1;
}
