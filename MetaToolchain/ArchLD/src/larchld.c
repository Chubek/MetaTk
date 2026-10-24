#include "ArchLD.h"
#include <lua.h>
#include <lauxlib.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct { ArchLD *ld; } LuaLD;
static LuaLD *check_ld(lua_State *L) { return (LuaLD *)luaL_checkudata(L, 1, "larchld.linker"); }
static int l_new(lua_State *L) { const char *cat=luaL_optstring(L,1,NULL), *target=luaL_optstring(L,2,"ELF64"); lua_Integer base=luaL_optinteger(L,3,target && target[0]=='E' && target[4]=='3' ? 0x08049000 : 0x401000); LuaLD *u=lua_newuserdatauv(L,sizeof *u,0); u->ld=archld_new(cat,target,(uint64_t)base); if(!u->ld) return luaL_error(L,"archld: allocation or catalogue failure"); luaL_getmetatable(L,"larchld.linker"); lua_setmetatable(L,-2); return 1; }
static int l_gc(lua_State *L){ LuaLD*u=check_ld(L); if(u->ld){archld_free(u->ld);u->ld=NULL;} return 0; }
static int l_add(lua_State *L){ LuaLD*u=check_ld(L); const char*p=luaL_checkstring(L,2); if(!archld_add(u->ld,p)) return luaL_error(L,"%s",archld_error(u->ld)); lua_pushboolean(L,1); return 1; }
static int l_link(lua_State *L){ LuaLD*u=check_ld(L); if(!archld_link(u->ld)) return luaL_error(L,"%s",archld_error(u->ld)); lua_pushboolean(L,1); return 1; }
static int l_write(lua_State *L){ LuaLD*u=check_ld(L); const char*p=luaL_checkstring(L,2); lua_Integer entry=luaL_optinteger(L,3,0); if(!archld_write_elf(u->ld,p,(uint64_t)entry)) return luaL_error(L,"%s",archld_error(u->ld)); lua_pushboolean(L,1); return 1; }
static int l_symbol(lua_State *L){ LuaLD*u=check_ld(L); const char*n=luaL_checkstring(L,2); uint64_t a; if(!archld_symbol(u->ld,n,&a)){lua_pushnil(L);return 1;} lua_pushinteger(L,(lua_Integer)a); return 1; }
static int l_error(lua_State *L){ lua_pushstring(L,archld_error(check_ld(L)->ld)); return 1; }
static const luaL_Reg module_methods[]={{"new",l_new},{NULL,NULL}};
static const luaL_Reg methods[]={{"add",l_add},{"link",l_link},{"write_elf",l_write},{"symbol",l_symbol},{"error",l_error},{"__gc",l_gc},{NULL,NULL}};
int luaopen_larchld(lua_State *L){ luaL_newmetatable(L,"larchld.linker"); lua_pushvalue(L,-1); lua_setfield(L,-2,"__index"); luaL_setfuncs(L,methods,0); lua_pop(L,1); luaL_newlib(L,module_methods); return 1; }
