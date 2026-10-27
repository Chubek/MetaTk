#include "lmtex.h"
#include <lua.h>
#include <lauxlib.h>
#define ENGINE_TYPE "metatex.engine"
#define RESULT_TYPE "metatex.result"
static int destroy_engine(lua_State* L) {
    lmtex_engine** engine = (lmtex_engine**)luaL_checkudata(L, 1, ENGINE_TYPE);
    lmtex_destroy(*engine); *engine = NULL; return 0;
}
static int release_result(lua_State* L) {
    lmtex_release((lmtex_result*)luaL_checkudata(L, 1, RESULT_TYPE)); return 0;
}
static int new_engine(lua_State* L) {
    lmtex_engine** engine = (lmtex_engine**)lua_newuserdata(L, sizeof(*engine));
    *engine = NULL; luaL_setmetatable(L, ENGINE_TYPE);
    *engine = lmtex_create();
    if (!*engine) return luaL_error(L, "cannot allocate MetaTeX engine");
    return 1;
}
static int compile_source(lua_State* L, int fragment) {
    lmtex_engine** engine = (lmtex_engine**)luaL_checkudata(L, 1, ENGINE_TYPE);
    size_t size = 0; const char* source = luaL_checklstring(L, 2, &size);
    const char* device = luaL_optstring(L, 3, "html");
    const char* filename = luaL_optstring(L, 4, "<lua>");
    if (!*engine) return luaL_error(L, "MetaTeX engine is closed");
    /* Anchor results in GC-managed userdata before calling allocating Lua APIs:
       Lua's allocation-error longjmp must not leak the C++ result buffers. */
    lmtex_result* result = (lmtex_result*)lua_newuserdata(L, sizeof(*result));
    result->output = NULL; result->diagnostics = NULL; result->output_size = 0; result->status = 0;
    luaL_setmetatable(L, RESULT_TYPE);
    *result = lmtex_compile(*engine, source, size, filename, device, fragment);
    if (result->status) lua_pushnil(L);
    else lua_pushlstring(L, result->output, result->output_size);
    lua_pushstring(L, result->diagnostics ? result->diagnostics : "MetaTeX allocation failure");
    lmtex_release(result); return 2;
}
static int compile_document(lua_State* L) { return compile_source(L, 0); }
static int compile_fragment(lua_State* L) { return compile_source(L, 1); }
int luaopen_lmtex(lua_State* L) {
    const luaL_Reg methods[] = {{"compile", compile_document}, {"fragment", compile_fragment}, {"close", destroy_engine}, {"__gc", destroy_engine}, {NULL, NULL}};
    const luaL_Reg module[] = {{"new", new_engine}, {NULL, NULL}};
    luaL_newmetatable(L, RESULT_TYPE); lua_pushcfunction(L, release_result); lua_setfield(L, -2, "__gc"); lua_pop(L, 1);
    luaL_newmetatable(L, ENGINE_TYPE); luaL_setfuncs(L, methods, 0);
    lua_pushvalue(L, -1); lua_setfield(L, -2, "__index"); lua_pop(L, 1);
    luaL_newlib(L, module); lua_pushliteral(L, "0.1.0"); lua_setfield(L, -2, "version"); return 1;
}
