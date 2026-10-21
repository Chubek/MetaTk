# luainit.q — lua C API primitives.
@module luainit
@version 1.0
@param PREFIX luainit
@raw {
#ifdef __cplusplus
extern "C" {
#endif
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#ifdef __cplusplus
}
#endif
}
@guard

@fn new {
static inline lua_State * ${PREFIX}_new(void) {
    return luaL_newstate();
}
}

@fn openlibs {
static inline void ${PREFIX}_openlibs(lua_State *L) {
    luaL_openlibs(L);
}
}

@fn close {
static inline void ${PREFIX}_close(lua_State *L) {
    if (L) lua_close(L);
}
}

