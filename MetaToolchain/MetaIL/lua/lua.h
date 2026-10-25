#ifndef METAIL_HOST_H
#define METAIL_HOST_H
#include "lua_compat.h"
int metail_lua_main(int argc, char **argv);
int metail_find_root(const char *argv0, const char *home, char *out, size_t size);
void lkopt_register(lua_State *L);
int luaopen_metanum_native(lua_State *L);
#endif
