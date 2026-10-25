/* MetaIL executable: real Lua 5.4 interpreter, sandboxing optk via Lua environment.
   The project never substitutes a toy Lua parser for Lua itself. */
#include "lua.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
static int is_file(const char *name) {
    struct stat st;
    return stat(name, &st) == 0 && S_ISREG(st.st_mode);
}
static int root_is_valid(const char *base) {
    char path[PATH_MAX];
    int n = snprintf(path, sizeof path, "%s/lua/metail/cli.lua", base);
    return n > 0 && (size_t)n < sizeof path && is_file(path);
}
int metail_find_root(const char *argv0, const char *home, char *out, size_t size) {
    if (home) {
        if (strlen(home)>=size || !root_is_valid(home)) return 0;
        memcpy(out,home,strlen(home)+1); return 1;
    }
    const char *env = getenv("METAIL_HOME");
    if (env) return metail_find_root(argv0,env,out,size);
    if (root_is_valid(".")) { snprintf(out,size,"."); return 1; }
    /* Compiled build and installed binaries: ../ relative to exe. */
    const char *slash = argv0 ? strrchr(argv0, '/') : NULL;
    if (slash) {
        size_t n = (size_t)(slash - argv0);
        if (n + 4 < size) {
            memcpy(out, argv0, n); out[n]=0;
            if (root_is_valid(out)) return 1;
            strncat(out, "/..", size-strlen(out)-1);
            if (root_is_valid(out)) return 1;
            /* Standard CMake install layout: <prefix>/bin/metail and
               <prefix>/share/metail/lua/metail/cli.lua. */
            snprintf(out, size, "%.*s/../share/metail", (int)n, argv0);
            if (root_is_valid(out)) return 1;
        }
    }
#ifdef METAIL_INSTALL_ROOT
    if (strlen(METAIL_INSTALL_ROOT)<size && root_is_valid(METAIL_INSTALL_ROOT)) {
        snprintf(out,size,"%s",METAIL_INSTALL_ROOT); return 1;
    }
#endif
#ifdef METAIL_SOURCE_ROOT
    if (root_is_valid(METAIL_SOURCE_ROOT)) { snprintf(out,size,"%s",METAIL_SOURCE_ROOT); return 1; }
#endif
    return 0;
}
int metail_lua_main(int argc, char **argv) {
    char root[PATH_MAX], entry[PATH_MAX];
    if (!metail_find_root(argv[0], NULL, root, sizeof root)) {
        fputs("metail: cannot locate lua/metail/cli.lua (set METAIL_HOME)\n", stderr);
        return 2;
    }
    lua_State *L=luaL_newstate();
    if (!L) { fputs("metail: Lua state allocation failed\n",stderr); return 2; }
    luaL_openlibs(L);
    lkopt_register(L);
    luaL_requiref(L,"metanum_native",luaopen_metanum_native,0);
    lua_settop(L,0);
    lua_pushstring(L,root); lua_setglobal(L,"_METAIL_ROOT");
    lua_createtable(L,argc,0);
    for (int i=0;i<argc;++i) {
        lua_pushstring(L,argv[i]); lua_rawseti(L,-2,i);
    }
    lua_setglobal(L,"arg");
    int n=snprintf(entry,sizeof entry,"%s/lua/metail/cli.lua",root);
    if(n<0 || (size_t)n>=sizeof entry) {fputs("metail: entry path too long\n",stderr);lua_close(L);return 2;}
    int status=luaL_loadfilex(L,entry,"t");
    if (status==LUA_OK) status=lua_pcallk(L,0,1,0,0,NULL);
    if(status!=LUA_OK) {
        const char *error=lua_tolstring(L,-1,NULL);
        fprintf(stderr,"metail: %s\n",error ? error : "Lua execution error");
        lua_close(L); return 1;
    }
    int ok=0; lua_Integer code=lua_tointegerx(L,-1,&ok);
    lua_close(L);
    return ok && code>=0 && code<=255 ? (int)code : 0;
}
