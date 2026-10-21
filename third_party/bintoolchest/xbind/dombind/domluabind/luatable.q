# luatable.q — lua C API primitives.
@module luatable
@version 1.0
@param PREFIX luatable
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
static inline void ${PREFIX}_new(lua_State *L, int array, int records) {
    lua_createtable(L, array, records);
}
}

@fn get {
static inline int ${PREFIX}_get(lua_State *L, int i, const char *key) {
    return lua_getfield(L, i, key);
}
}

@fn set {
static inline void ${PREFIX}_set(lua_State *L, int i, const char *key) {
    lua_setfield(L, i, key);
}
}

@fn get_index {
static inline int ${PREFIX}_get_index(lua_State *L, int i, lua_Integer key) {
    return lua_geti(L, i, key);
}
}

@fn set_index {
static inline void ${PREFIX}_set_index(lua_State *L, int i, lua_Integer key) {
    lua_seti(L, i, key);
}
}

@fn size {
static inline size_t ${PREFIX}_size(lua_State *L, int i) {
    return lua_rawlen(L, i);
}
}

@fn next {
static inline int ${PREFIX}_next(lua_State *L, int i) {
    return lua_next(L, i);
}
}

