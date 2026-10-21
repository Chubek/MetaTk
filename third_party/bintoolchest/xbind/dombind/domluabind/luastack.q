# luastack.q — lua C API primitives.
@module luastack
@version 1.0
@param PREFIX luastack
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

@fn size {
static inline int ${PREFIX}_size(lua_State *L) {
    return lua_gettop(L);
}
}

@fn resize {
static inline void ${PREFIX}_resize(lua_State *L, int n) {
    lua_settop(L, n);
}
}

@fn reserve {
static inline int ${PREFIX}_reserve(lua_State *L, int n) {
    return lua_checkstack(L, n);
}
}

@fn absolute {
static inline int ${PREFIX}_absolute(lua_State *L, int i) {
    return lua_absindex(L, i);
}
}

@fn copy {
static inline void ${PREFIX}_copy(lua_State *L, int i) {
    lua_pushvalue(L, i);
}
}

@fn pop {
static inline void ${PREFIX}_pop(lua_State *L, int n) {
    lua_pop(L, n);
}
}

@fn type {
static inline int ${PREFIX}_type(lua_State *L, int i) {
    return lua_type(L, i);
}
}

