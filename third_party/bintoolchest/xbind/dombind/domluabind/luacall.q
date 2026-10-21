# luacall.q — lua C API primitives.
@module luacall
@version 1.0
@param PREFIX luacall
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

@fn protected {
static inline int ${PREFIX}_protected(lua_State *L, int nargs, int results, int handler) {
    return lua_pcall(L, nargs, results, handler);
}
}

@fn load {
static inline int ${PREFIX}_load(lua_State *L, const char *s, size_t n, const char *name) {
    return luaL_loadbufferx(L, s, n, name, NULL);
}
}

@fn eval {
static inline int ${PREFIX}_eval(lua_State *L, const char *s, size_t n, int results) {
    int status = luaL_loadbufferx(L, s, n, "domluabind", NULL);
    return status == LUA_OK ? lua_pcall(L, 0, results, 0) : status;
}
}

@fn push {
static inline void ${PREFIX}_push(lua_State *L, lua_CFunction fn, int upvalues) {
    lua_pushcclosure(L, fn, upvalues);
}
}

@fn get_global {
static inline int ${PREFIX}_get_global(lua_State *L, const char *name) {
    return lua_getglobal(L, name);
}
}

@fn set_global {
static inline void ${PREFIX}_set_global(lua_State *L, const char *name) {
    lua_setglobal(L, name);
}
}

