# luaconv.q — lua C API primitives.
@module luaconv
@version 1.0
@param PREFIX luaconv
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

@fn push_integer {
static inline void ${PREFIX}_push_integer(lua_State *L, lua_Integer v) {
    lua_pushinteger(L, v);
}
}

@fn integer {
static inline lua_Integer ${PREFIX}_integer(lua_State *L, int i, int *ok) {
    return lua_tointegerx(L, i, ok);
}
}

@fn push_number {
static inline void ${PREFIX}_push_number(lua_State *L, lua_Number v) {
    lua_pushnumber(L, v);
}
}

@fn number {
static inline lua_Number ${PREFIX}_number(lua_State *L, int i, int *ok) {
    return lua_tonumberx(L, i, ok);
}
}

@fn push_boolean {
static inline void ${PREFIX}_push_boolean(lua_State *L, int v) {
    lua_pushboolean(L, v);
}
}

@fn boolean {
static inline int ${PREFIX}_boolean(lua_State *L, int i) {
    return lua_toboolean(L, i);
}
}

@fn push_nil {
static inline void ${PREFIX}_push_nil(lua_State *L) {
    lua_pushnil(L);
}
}

@fn push_string {
static inline const char * ${PREFIX}_push_string(lua_State *L, const char *s, size_t n) {
    return lua_pushlstring(L, s, n);
}
}

@fn string {
static inline const char * ${PREFIX}_string(lua_State *L, int i, size_t *n) {
    return lua_tolstring(L, i, n);
}
}

