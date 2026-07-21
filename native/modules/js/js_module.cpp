#include "js_module.h"
#include "lualib.h"

int luaopen_js(lua_State *L) {
    lua_newtable(L);
    // Placeholder functions will be added in Phase 11
    return 1;
}
