/* Public Lua 5.4 API declarations used by this application.
   This is NOT a replacement Lua runtime. Link against the genuine Lua 5.4 library.
   Install the official development headers to use upstream lua.h instead if desired. */
#ifndef METAIL_LUA_COMPAT_H
#define METAIL_LUA_COMPAT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct lua_State lua_State;
typedef int (*lua_CFunction)(lua_State *L);
typedef long long lua_Integer;
typedef unsigned long long lua_Unsigned;
typedef double lua_Number;
typedef ptrdiff_t lua_KContext;
typedef int (*lua_KFunction)(lua_State *, int, lua_KContext);
#define LUA_OK 0
#define LUA_TNIL 0
#define LUA_TBOOLEAN 1
#define LUA_TNUMBER 3
#define LUA_TSTRING 4
#define LUA_TTABLE 5
#define LUA_TFUNCTION 6
#define LUA_REGISTRYINDEX (-1001000)
lua_State *luaL_newstate(void);
void luaL_openlibs(lua_State *L);
void lua_close(lua_State *L);
int luaL_loadfilex(lua_State *L, const char *filename, const char *mode);
int lua_pcallk(lua_State *L, int nargs, int nresults, int errfunc, lua_KContext ctx, lua_KFunction k);
const char *lua_tolstring(lua_State *L, int idx, size_t *len);
lua_Integer lua_tointegerx(lua_State *L, int idx, int *isnum);
int lua_isinteger(lua_State *L, int idx);
int lua_type(lua_State *L, int idx);
int lua_gettop(lua_State *L);
void lua_settop(lua_State *L, int idx);
lua_Number lua_tonumberx(lua_State *L, int idx, int *isnum);
int lua_toboolean(lua_State *L, int idx);
size_t lua_rawlen(lua_State *L, int idx);
int lua_rawgeti(lua_State *L, int idx, lua_Integer n);
int lua_next(lua_State *L, int idx);
void *lua_newuserdatauv(lua_State *L, size_t size, int nuvalue);
void lua_pushnumber(lua_State *L, lua_Number n);
const char *lua_pushlstring(lua_State *L, const char *s, size_t len);
void luaL_requiref(lua_State *L, const char *name, lua_CFunction openf, int glb);
void lua_pushinteger(lua_State *L, lua_Integer n);
void lua_pushboolean(lua_State *L, int b);
void lua_pushnil(lua_State *L);
const char *lua_pushstring(lua_State *L, const char *s);
void lua_pushcclosure(lua_State *L, lua_CFunction fn, int n);
void lua_createtable(lua_State *L, int narr, int nrec);
void lua_setfield(lua_State *L, int idx, const char *k);
void lua_setglobal(lua_State *L, const char *name);
void lua_rawseti(lua_State *L, int idx, lua_Integer n);
const char *luaL_checklstring(lua_State *L, int arg, size_t *len);
lua_Integer luaL_checkinteger(lua_State *L, int arg);
int luaL_error(lua_State *L, const char *fmt, ...);
#ifdef __cplusplus
}
#endif
#endif
