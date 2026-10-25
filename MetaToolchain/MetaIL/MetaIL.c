#include "MetaIL.h"
#include "lua/lua.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct metail_context {
    lua_State *lua;
    char root[4096];
    char error[2048];
};
typedef struct request {
    metail_context *context;
    const char *operation, *source, *name, *passes;
    int rounds;
    size_t index;
} request;

static metail_status fail(metail_context *c, metail_status status, const char *message) {
    if (c) snprintf(c->error,sizeof c->error,"%s",message);
    return status;
}
static int absolute_path(const char *path, char *out, size_t size) {
    if (path[0]=='/') {
        if (strlen(path)>=size) return 0;
        memcpy(out,path,strlen(path)+1);return 1;
    }
    char cwd[4096];
    if (!getcwd(cwd,sizeof cwd)) return 0;
    int n=snprintf(out,size,"%s/%s",cwd,path);
    return n>=0 && (size_t)n<size;
}

/* All allocating Lua API calls execute below a protected C trampoline. In
 * particular initialization failures must not reach Lua's panic/abort path. */
static int initialize(lua_State *L) {
    metail_context *c=(metail_context *)lua_touserdata(L,1);
    char entry[4096];
    int n=snprintf(entry,sizeof entry,"%s/lua/metail/api.lua",c->root);
    if (n<0 || (size_t)n>=sizeof entry) return luaL_error(L,"runtime path too long");
    luaL_openlibs(L);
    lkopt_register(L);
    luaL_requiref(L,"metanum_native",luaopen_metanum_native,0);
    lua_settop(L,1);
    lua_pushstring(L,c->root);lua_setglobal(L,"_METAIL_ROOT");
    lua_getglobal(L,"package");
    lua_getfield(L,-1,"path");
    const char *old=lua_tolstring(L,-1,NULL);
    /* Use Lua's string.format so no C allocation is live across a Lua error. */
    lua_getglobal(L,"string");lua_getfield(L,-1,"format");
    lua_pushstring(L,"%s/lua/?.lua;%s/lua/?/init.lua;%s");
    lua_pushstring(L,c->root);lua_pushstring(L,c->root);lua_pushstring(L,old);
    if (lua_pcallk(L,4,1,0,0,NULL)!=LUA_OK)
        return luaL_error(L,"%s",lua_tolstring(L,-1,NULL));
    lua_setfield(L,2,"path");
    lua_settop(L,0);
    if (luaL_loadfilex(L,entry,"t")!=LUA_OK || lua_pcallk(L,0,1,0,0,NULL)!=LUA_OK)
        return luaL_error(L,"%s",lua_tolstring(L,-1,NULL));
    lua_setglobal(L,"_METAIL_API");
    return 0;
}

metail_context *metail_create(const char *home, char *error, size_t error_size) {
    metail_context *c=(metail_context *)calloc(1,sizeof *c);
    const char *message="MetaIL context allocation failed";
    if (error && error_size) error[0]='\0';
    if (c) {
        char root[4096];
        if (!metail_find_root(NULL,home,c->root,sizeof c->root))
            message="cannot locate MetaIL runtime (set METAIL_HOME)";
        else if (!absolute_path(c->root,root,sizeof root)) message="cannot resolve runtime path";
        else if (!(c->lua=luaL_newstate())) message="Lua state allocation failed";
        else {
            memcpy(c->root,root,strlen(root)+1);
            lua_pushcclosure(c->lua,initialize,0);
            lua_pushlightuserdata(c->lua,c);
            if (lua_pcallk(c->lua,1,0,0,0,NULL)==LUA_OK) return c;
            message=lua_tolstring(c->lua,-1,NULL);
            if (!message) message="Lua initialization failed";
        }
    }
    if (error && error_size) snprintf(error,error_size,"%s",message);
    metail_destroy(c);
    return NULL;
}
void metail_destroy(metail_context *c) {
    if (c) { if (c->lua) lua_close(c->lua);free(c); }
}
void metail_free(void *p) { free(p); }
const char *metail_last_error(const metail_context *c) {
    return c ? c->error : "invalid MetaIL context";
}

static int dispatch(lua_State *L) {
    request *r=(request *)lua_touserdata(L,1);
    lua_getglobal(L,"_METAIL_API");
    lua_getfield(L,-1,r->operation);
    int nargs=0, nresults=0;
    if (strcmp(r->operation,"load_kernels")==0) {
        lua_pushstring(L,r->source);nargs=1;
    } else if (strcmp(r->operation,"kernel_count")==0 || strcmp(r->operation,"kernels_json")==0) {
        nresults=1;
    } else if (strcmp(r->operation,"kernel_info")==0) {
        lua_pushinteger(L,(lua_Integer)r->index+1);nargs=1;nresults=8;
    } else {
        lua_pushstring(L,r->source);lua_pushstring(L,r->name ? r->name : "<memory>");nargs=2;
        if (strcmp(r->operation,"optimize")==0 || strcmp(r->operation,"analyze")==0) {
            lua_pushstring(L,r->passes);nargs=3;
            if (strcmp(r->operation,"optimize")==0) {
                lua_pushinteger(L,r->rounds);nargs=4;nresults=2;
            } else nresults=1;
        } else nresults=strcmp(r->operation,"emit")==0 ? 1 : 0;
    }
    if (lua_pcallk(L,nargs,nresults,0,0,NULL)!=LUA_OK) {
        const char *message=lua_tolstring(L,-1,NULL);
        return luaL_error(L,"%s",message ? message : "Lua execution failed");
    }
    return nresults;
}
static metail_status invoke(request *r, int results) {
    metail_context *c=r->context;
    if (!c) return METAIL_INVALID_ARGUMENT;
    c->error[0]='\0';
    lua_settop(c->lua,0);
    lua_pushcclosure(c->lua,dispatch,0);lua_pushlightuserdata(c->lua,r);
    int status=lua_pcallk(c->lua,1,results,0,0,NULL);
    if (status!=LUA_OK) {
        const char *message=lua_tolstring(c->lua,-1,NULL);
        fail(c,status==4 ? METAIL_OUT_OF_MEMORY : METAIL_RUNTIME_ERROR,
             message ? message : "Lua execution failed");
        lua_settop(c->lua,0);
        return status==4 ? METAIL_OUT_OF_MEMORY : METAIL_RUNTIME_ERROR;
    }
    return METAIL_OK;
}
static char *copy_string(lua_State *L, int index) {
    size_t size=0;
    const char *s=lua_tolstring(L,index,&size);
    if (!s || size==SIZE_MAX) return NULL;
    char *copy=(char *)malloc(size+1);
    if (copy) { memcpy(copy,s,size);copy[size]='\0'; }
    return copy;
}
static metail_status text_operation(metail_context *c, const char *op, const char *source,
    const char *name, const char *passes, int rounds, char **output, char **stats) {
    if (output) *output=NULL;
    if (stats) *stats=NULL;
    if (!c || !source || !output || output==stats)
        return fail(c,METAIL_INVALID_ARGUMENT,"source and distinct output slots are required");
    if (rounds<1 || rounds>12) return fail(c,METAIL_INVALID_ARGUMENT,"rounds must be 1..12");
    request r={c,op,source,name,passes,rounds,0};
    int results=strcmp(op,"optimize")==0 ? 2 : 1;
    metail_status status=invoke(&r,results);
    if (status!=METAIL_OK) return status;
    char *text=copy_string(c->lua,1);
    char *changes=stats ? copy_string(c->lua,2) : NULL;
    lua_settop(c->lua,0);
    if (!text || (stats && !changes)) {
        free(text);free(changes);return fail(c,METAIL_OUT_OF_MEMORY,"result allocation failed");
    }
    *output=text;if (stats) *stats=changes;
    return METAIL_OK;
}
metail_status metail_check(metail_context *c, const char *source, const char *name) {
    if (!source) return fail(c,METAIL_INVALID_ARGUMENT,"source is required");
    request r={c,"check",source,name,NULL,0,0};return invoke(&r,0);
}
metail_status metail_emit(metail_context *c, const char *s, const char *n, char **out) {
    return text_operation(c,"emit",s,n,NULL,1,out,NULL);
}
metail_status metail_optimize(metail_context *c, const char *s, const char *n,
    const char *p, int rounds, char **out, char **stats) {
    return text_operation(c,"optimize",s,n,p,rounds,out,stats);
}
metail_status metail_analyze(metail_context *c, const char *s, const char *n, const char *p, char **out) {
    return text_operation(c,"analyze",s,n,p,1,out,NULL);
}
metail_status metail_load_kernels(metail_context *c, const char *directory) {
    if (!directory || !*directory) return fail(c,METAIL_INVALID_ARGUMENT,"kernel directory is required");
    char path[4096];
    if (!absolute_path(directory,path,sizeof path)) return fail(c,METAIL_INVALID_ARGUMENT,"cannot resolve kernel directory");
    request r={c,"load_kernels",path,NULL,NULL,0,0};return invoke(&r,0);
}
metail_status metail_kernel_count(metail_context *c, size_t *count) {
    if (!count) return fail(c,METAIL_INVALID_ARGUMENT,"count output is required");
    *count=0;
    request r={c,"kernel_count",NULL,NULL,NULL,0,0};
    metail_status status=invoke(&r,1);
    if (status==METAIL_OK) *count=(size_t)lua_tointegerx(c->lua,1,NULL);
    return status;
}
metail_status metail_kernel_get(metail_context *c, size_t index, metail_kernel_info *info) {
    if (info) memset(info,0,sizeof *info);
    if (!info || index>=(size_t)INT64_MAX) return fail(c,METAIL_INVALID_ARGUMENT,"invalid kernel index/output");
    request r={c,"kernel_info",NULL,NULL,NULL,0,index};
    metail_status status=invoke(&r,8);
    if (status==METAIL_OK) {
        info->name=lua_tolstring(c->lua,1,NULL);info->path=lua_tolstring(c->lua,2,NULL);
        info->kind=lua_tolstring(c->lua,3,NULL);info->implementation=lua_tolstring(c->lua,4,NULL);
        info->description=lua_tolstring(c->lua,5,NULL);info->keywords=lua_tolstring(c->lua,6,NULL);
        info->tags=lua_tolstring(c->lua,7,NULL);info->uses=lua_tolstring(c->lua,8,NULL);
    }
    return status;
}
metail_status metail_kernels_json(metail_context *c, char **out) {
    return text_operation(c,"kernels_json","",NULL,NULL,1,out,NULL);
}
int metail_cli_main(int argc, char **argv) {
    if (argc<1 || !argv) return 2;
    for (int i=0;i<argc;++i) if (!argv[i]) return 2;
    return metail_lua_main(argc,argv);
}
