#ifndef METAIL_HOST_H
#define METAIL_HOST_H
#include "lua_compat.h"
int metail_lua_main(int argc, char **argv);
void lkopt_register(lua_State *L);
int luaopen_metanum_native(lua_State *L);
#endif
