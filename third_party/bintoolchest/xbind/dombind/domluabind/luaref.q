# luaref.q — lua C API primitives.
@module luaref
@version 1.0
@param PREFIX luaref
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

@fn retain {
static inline int ${PREFIX}_retain(lua_State *L, int i) {
    lua_pushvalue(L, i); return luaL_ref(L, LUA_REGISTRYINDEX);
}
}

@fn release {
static inline void ${PREFIX}_release(lua_State *L, int ref) {
    luaL_unref(L, LUA_REGISTRYINDEX, ref);
}
}

@fn push {
static inline int ${PREFIX}_push(lua_State *L, int ref) {
    return lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
}
}

