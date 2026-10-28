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
    luaL_requiref(L,"weighstone_native",luaopen_weighstone_native,0);
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
        } else nresults=strcmp(r->operation,"emit")==0 || strcmp(r->operation,"linkage")==0 ? 1 : 0;
    }
    if (lua_pcallk(L,nargs,nresults,0,0,NULL)!=LUA_OK) {
        const char *message=lua_tolstring(L,-1,NULL);
        return luaL_error(L,"%s",message ? message : "Lua execution failed");
    }
    return nresults;
}
static metail_status protected_call(metail_context *c, lua_CFunction function, void *data, int results) {
    if (!c) return METAIL_INVALID_ARGUMENT;
    c->error[0]='\0';
    lua_settop(c->lua,0);
    lua_pushcclosure(c->lua,function,0);lua_pushlightuserdata(c->lua,data);
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
static metail_status invoke(request *r, int results) {
    return protected_call(r->context,dispatch,r,results);
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
metail_status metail_linkage(metail_context *c, const char *s, const char *n, char **out) {
    return text_operation(c,"linkage",s,n,NULL,1,out,NULL);
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

/* Native structured-IR bridge. No MetaIL text is generated or parsed here,
 * except in the explicitly textual metail_parse operation. */
static const char *const operator_names[]={
    "add","sub","mul","div","mod","band","bor","bxor","shl","shr",
    "eq","ne","lt","le","gt","ge","land","lor","neg","bnot","lnot","select"
};
int metail_operator_arity(const char *op) {
    if (!op) return -1;
    for (size_t i=0;i<sizeof operator_names/sizeof *operator_names;++i)
        if (!strcmp(op,operator_names[i])) return i<18 ? 2 : i<21 ? 1 : 3;
    return -1;
}
static const char *const statement_names[]={"let","return","eval","assume","label","goto","br"};
typedef struct ir_allocation {
    struct ir_allocation *next;
    max_align_t alignment;
    unsigned char data[];
} ir_allocation;
typedef struct ir_owner {
    metail_module module; /* First member: public read-only snapshot. */
    ir_allocation *allocations;
} ir_owner;
typedef struct ir_request {
    const char *operation, *source, *name, *passes;
    const metail_module *input;
    ir_owner *owner;
    int rounds, oom;
    size_t items, bytes;
} ir_request;
void metail_module_free(metail_module *module) {
    if (!module) return;
    ir_owner *owner=(ir_owner *)module;
    ir_allocation *a=owner->allocations;
    while (a) { ir_allocation *next=a->next;free(a);a=next; }
    free(owner);
}
static void ir_items(lua_State *L, ir_request *r, size_t count) {
    if (count>200000-r->items) luaL_error(L,"IR item limit exceeded");
    r->items+=count;
}
static void ir_stack(lua_State *L) {
    if (!lua_checkstack(L,16)) luaL_error(L,"IR Lua stack limit exceeded");
}
static void *ir_allocate(lua_State *L, ir_request *r, size_t count, size_t size) {
    if (!count) return NULL;
    if (count>(SIZE_MAX-sizeof(ir_allocation))/size) luaL_error(L,"IR allocation overflow");
    ir_allocation *a=(ir_allocation *)calloc(1,sizeof *a+count*size);
    if (!a) { r->oom=1;luaL_error(L,"IR allocation failed");return NULL; }
    a->next=r->owner->allocations;r->owner->allocations=a;
    return a->data;
}
static void ir_string_budget(lua_State *L, ir_request *r, const char *s) {
    if (!s) luaL_error(L,"required IR string is NULL");
    size_t length=strlen(s);
    if (length>8*1024*1024-r->bytes) luaL_error(L,"IR string limit exceeded");
    r->bytes+=length;
}
static void ir_string(lua_State *L, ir_request *r, const char *key, const char *s) {
    ir_string_budget(L,r,s);lua_pushstring(L,s);lua_setfield(L,-2,key);
}
static void ir_array(lua_State *L, const void *data, size_t count) {
    if (count>200000 || (count && !data)) luaL_error(L,"invalid IR array/count");
    lua_createtable(L,(int)count,0);
}
static void push_expr(lua_State *L, ir_request *r, const metail_expr *e, unsigned depth) {
    ir_stack(L);ir_items(L,r,1);
    if (!e || depth>256) luaL_error(L,"NULL expression, cycle, or IR depth exceeds 256");
    lua_createtable(L,0,4);
    switch (e->kind) {
    case METAIL_INT:
        ir_string(L,r,"tag","int");lua_pushinteger(L,(lua_Integer)e->value);lua_setfield(L,-2,"value");break;
    case METAIL_VAR:
        ir_string(L,r,"tag","var");ir_string(L,r,"name",e->name);break;
    case METAIL_OP: case METAIL_CALL:
        ir_string(L,r,"tag",e->kind==METAIL_OP ? "op" : "call");
        if (e->kind==METAIL_OP) {
            int arity=metail_operator_arity(e->op);
            if (arity<0 || (size_t)arity!=e->arg_count) luaL_error(L,"invalid operator or arity");
            ir_string(L,r,"op",e->op);
        } else ir_string(L,r,"name",e->name);
        ir_array(L,e->args,e->arg_count);
        for (size_t i=0;i<e->arg_count;++i) {
            push_expr(L,r,e->args[i],depth+1);lua_rawseti(L,-2,(lua_Integer)i+1);
        }
        lua_setfield(L,-2,"args");break;
    default: luaL_error(L,"invalid expression kind");
    }
}
static void push_module(lua_State *L, ir_request *r) {
    const metail_module *m=r->input;
    lua_createtable(L,0,2);
    if (m->name) ir_string(L,r,"module",m->name);
    ir_items(L,r,m->external_count);ir_array(L,m->externals,m->external_count);
    for (size_t i=0;i<m->external_count;++i) {
        const metail_external *f=&m->externals[i];
        lua_createtable(L,0,7);ir_string(L,r,"name",f->name);
        ir_string(L,r,"symbol",f->symbol ? f->symbol : f->name);
        ir_string(L,r,"library",f->library);ir_string(L,r,"result",f->result ? f->result : "i64");
        if (f->variadic!=0 && f->variadic!=1) luaL_error(L,"external variadic flag must be 0 or 1");
        lua_pushboolean(L,f->variadic);lua_setfield(L,-2,"variadic");
        ir_items(L,r,f->param_count);ir_array(L,f->params,f->param_count);
        for (size_t j=0;j<f->param_count;++j) {
            ir_string_budget(L,r,f->params[j]);lua_pushstring(L,f->params[j]);lua_rawseti(L,-2,(lua_Integer)j+1);
        }
        lua_setfield(L,-2,"params");lua_rawseti(L,-2,(lua_Integer)i+1);
    }
    lua_setfield(L,-2,"externals");
    ir_items(L,r,m->function_count);ir_array(L,m->functions,m->function_count);
    for (size_t i=0;i<m->function_count;++i) {
        const metail_function *f=&m->functions[i];
        ir_stack(L);lua_createtable(L,0,3);ir_string(L,r,"name",f->name);
        ir_items(L,r,f->param_count);ir_array(L,f->params,f->param_count);
        for (size_t j=0;j<f->param_count;++j) {
            ir_string_budget(L,r,f->params[j]);lua_pushstring(L,f->params[j]);lua_rawseti(L,-2,(lua_Integer)j+1);
        }
        lua_setfield(L,-2,"params");
        ir_items(L,r,f->statement_count);ir_array(L,f->statements,f->statement_count);
        for (size_t j=0;j<f->statement_count;++j) {
            const metail_stmt *s=&f->statements[j];
            if (s->kind<METAIL_LET || s->kind>METAIL_BRANCH) luaL_error(L,"invalid statement kind");
            lua_createtable(L,0,4);ir_string(L,r,"kind",statement_names[s->kind]);
            if (s->kind==METAIL_LET || s->kind==METAIL_LABEL) ir_string(L,r,"name",s->name);
            if (s->kind==METAIL_GOTO) ir_string(L,r,"target",s->target);
            if (s->kind==METAIL_BRANCH) { ir_string(L,r,"yes",s->yes);ir_string(L,r,"no",s->no); }
            if (s->kind<=METAIL_ASSUME || s->kind==METAIL_BRANCH) {
                push_expr(L,r,s->expr,1);lua_setfield(L,-2,"expr");
            }
            lua_rawseti(L,-2,(lua_Integer)j+1);
        }
        lua_setfield(L,-2,"statements");lua_rawseti(L,-2,(lua_Integer)i+1);
    }
    lua_setfield(L,-2,"functions");
}
static const char *read_string(lua_State *L, ir_request *r, int table, const char *key, int optional) {
    lua_getfield(L,table,key);
    if (optional && lua_type(L,-1)==LUA_TNIL) { lua_settop(L,-2);return NULL; }
    if (lua_type(L,-1)!=LUA_TSTRING) luaL_error(L,"invalid IR string field %s",key);
    size_t size=0;const char *s=lua_tolstring(L,-1,&size);
    ir_string_budget(L,r,s);
    char *out=(char *)ir_allocate(L,r,size+1,1);memcpy(out,s,size+1);
    lua_settop(L,-2);return out;
}
static size_t read_array(lua_State *L, int table, const char *key) {
    lua_getfield(L,table,key);
    if (lua_type(L,-1)!=LUA_TTABLE) luaL_error(L,"invalid IR array %s",key);
    size_t count=lua_rawlen(L,-1);
    if (count>200000) luaL_error(L,"IR array limit exceeded");
    return count;
}
static metail_expr *read_expr(lua_State *L, ir_request *r, int table, unsigned depth) {
    ir_stack(L);ir_items(L,r,1);
    if (depth>256) luaL_error(L,"IR expression depth exceeds 256");
    metail_expr *e=(metail_expr *)ir_allocate(L,r,1,sizeof *e);
    const char *tag=read_string(L,r,table,"tag",0);
    if (!strcmp(tag,"int")) {
        e->kind=METAIL_INT;lua_getfield(L,table,"value");
        e->value=(int64_t)lua_tointegerx(L,-1,NULL);lua_settop(L,-2);
    } else if (!strcmp(tag,"var")) {
        e->kind=METAIL_VAR;e->name=read_string(L,r,table,"name",0);
    } else {
        e->kind=!strcmp(tag,"op") ? METAIL_OP : METAIL_CALL;
        if (e->kind==METAIL_OP) e->op=read_string(L,r,table,"op",0);
        else e->name=read_string(L,r,table,"name",0);
        e->arg_count=read_array(L,table,"args");
        const metail_expr **args=(const metail_expr **)ir_allocate(L,r,e->arg_count,sizeof *args);
        e->args=args;
        for (size_t i=0;i<e->arg_count;++i) {
            lua_rawgeti(L,-1,(lua_Integer)i+1);args[i]=read_expr(L,r,lua_gettop(L),depth+1);lua_settop(L,-2);
        }
        lua_settop(L,-2);
    }
    return e;
}
static void read_module(lua_State *L, ir_request *r, int table) {
    r->items=0;r->bytes=0;
    metail_module *m=&r->owner->module;
    m->name=read_string(L,r,table,"module",1);
    m->external_count=read_array(L,table,"externals");ir_items(L,r,m->external_count);
    metail_external *externals=(metail_external *)ir_allocate(L,r,m->external_count,sizeof *externals);
    m->externals=externals;
    for (size_t i=0;i<m->external_count;++i) {
        lua_rawgeti(L,-1,(lua_Integer)i+1);int ft=lua_gettop(L);metail_external *f=&externals[i];
        f->name=read_string(L,r,ft,"name",0);f->symbol=read_string(L,r,ft,"symbol",0);
        f->library=read_string(L,r,ft,"library",0);f->result=read_string(L,r,ft,"result",0);
        lua_getfield(L,ft,"variadic");f->variadic=lua_toboolean(L,-1);lua_settop(L,-2);
        f->param_count=read_array(L,ft,"params");ir_items(L,r,f->param_count);
        const char **params=(const char **)ir_allocate(L,r,f->param_count,sizeof *params);f->params=params;
        for (size_t j=0;j<f->param_count;++j) {
            lua_rawgeti(L,-1,(lua_Integer)j+1);
            const char *s=lua_tolstring(L,-1,NULL);ir_string_budget(L,r,s);
            size_t size=strlen(s)+1;char *copy=(char *)ir_allocate(L,r,size,1);memcpy(copy,s,size);
            params[j]=copy;lua_settop(L,-2);
        }
        lua_settop(L,ft-1);
    }
    lua_settop(L,-2);
    m->function_count=read_array(L,table,"functions");ir_items(L,r,m->function_count);
    metail_function *functions=(metail_function *)ir_allocate(L,r,m->function_count,sizeof *functions);
    m->functions=functions;
    for (size_t i=0;i<m->function_count;++i) {
        ir_stack(L);lua_rawgeti(L,-1,(lua_Integer)i+1);int ft=lua_gettop(L);
        metail_function *f=&functions[i];f->name=read_string(L,r,ft,"name",0);
        f->param_count=read_array(L,ft,"params");ir_items(L,r,f->param_count);
        const char **params=(const char **)ir_allocate(L,r,f->param_count,sizeof *params);f->params=params;
        for (size_t j=0;j<f->param_count;++j) {
            lua_rawgeti(L,-1,(lua_Integer)j+1);
            const char *s=lua_tolstring(L,-1,NULL);ir_string_budget(L,r,s);
            size_t size=strlen(s)+1;char *copy=(char *)ir_allocate(L,r,size,1);memcpy(copy,s,size);
            params[j]=copy;lua_settop(L,-2);
        }
        lua_settop(L,-2);
        f->statement_count=read_array(L,ft,"statements");ir_items(L,r,f->statement_count);
        metail_stmt *statements=(metail_stmt *)ir_allocate(L,r,f->statement_count,sizeof *statements);f->statements=statements;
        for (size_t j=0;j<f->statement_count;++j) {
            lua_rawgeti(L,-1,(lua_Integer)j+1);int st=lua_gettop(L);metail_stmt *s=&statements[j];
            const char *kind=read_string(L,r,st,"kind",0);
            size_t k=0;while (k<7 && strcmp(statement_names[k],kind)) ++k;
            if (k==7) luaL_error(L,"invalid exported statement kind");
            s->kind=(metail_stmt_kind)k;
            if (s->kind==METAIL_LET || s->kind==METAIL_LABEL) s->name=read_string(L,r,st,"name",0);
            if (s->kind==METAIL_GOTO) s->target=read_string(L,r,st,"target",0);
            if (s->kind==METAIL_BRANCH) { s->yes=read_string(L,r,st,"yes",0);s->no=read_string(L,r,st,"no",0); }
            if (s->kind<=METAIL_ASSUME || s->kind==METAIL_BRANCH) {
                lua_getfield(L,st,"expr");s->expr=read_expr(L,r,lua_gettop(L),1);lua_settop(L,-2);
            }
            lua_settop(L,-2);
        }
        lua_settop(L,ft-1);
    }
    lua_settop(L,-2);
}
static int ir_dispatch(lua_State *L) {
    ir_request *r=(ir_request *)lua_touserdata(L,1);
    lua_getglobal(L,"_METAIL_API");lua_getfield(L,-1,r->operation);
    int nargs=1,results=1;
    if (!strcmp(r->operation,"parse")) {
        lua_pushstring(L,r->source);lua_pushstring(L,r->name ? r->name : "<memory>");nargs=2;
    } else {
        push_module(L,r);
        if (!strcmp(r->operation,"module_check")) results=0;
        if (!strcmp(r->operation,"module_analyze") || !strcmp(r->operation,"module_optimize")) {
            lua_pushstring(L,r->passes);nargs=2;
            if (!strcmp(r->operation,"module_optimize")) {
                lua_pushinteger(L,r->rounds);nargs=3;results=2;
            }
        }
    }
    if (lua_pcallk(L,nargs,results,0,0,NULL)!=LUA_OK) {
        const char *error=lua_tolstring(L,-1,NULL);return luaL_error(L,"%s",error ? error : "IR operation failed");
    }
    if (r->owner) {
        read_module(L,r,lua_gettop(L)-results+1);
        return results-1; /* optimize: return stats; parse: no Lua result */
    }
    return results;
}
static metail_status ir_run(metail_context *c, ir_request *r, metail_module **module, char **text) {
    if (module) *module=NULL;
    if (text) *text=NULL;
    if (!c || (!r->input && !r->source)) return fail(c,METAIL_INVALID_ARGUMENT,"IR/source and context are required");
    if (r->rounds<1 || r->rounds>12) return fail(c,METAIL_INVALID_ARGUMENT,"rounds must be 1..12");
    if (module) {
        r->owner=(ir_owner *)calloc(1,sizeof *r->owner);
        if (!r->owner) return fail(c,METAIL_OUT_OF_MEMORY,"module allocation failed");
    }
    metail_status status=protected_call(c,ir_dispatch,r,text ? 1 : 0);
    if (status==METAIL_OK && text) {
        *text=copy_string(c->lua,1);
        if (!*text) status=fail(c,METAIL_OUT_OF_MEMORY,"result allocation failed");
    }
    lua_settop(c->lua,0);
    if (status!=METAIL_OK) {
        if (r->owner) metail_module_free(&r->owner->module);
        return r->oom ? METAIL_OUT_OF_MEMORY : status;
    }
    if (module) *module=&r->owner->module;
    return METAIL_OK;
}
metail_status metail_parse(metail_context *c, const char *source, const char *name, metail_module **output) {
    if (!output) return fail(c,METAIL_INVALID_ARGUMENT,"module output required");
    ir_request r={.operation="parse",.source=source,.name=name,.rounds=1};
    return ir_run(c,&r,output,NULL);
}
metail_status metail_module_check(metail_context *c, const metail_module *module) {
    ir_request r={.operation="module_check",.input=module,.rounds=1};return ir_run(c,&r,NULL,NULL);
}
metail_status metail_module_emit(metail_context *c, const metail_module *module, char **output) {
    if (!output) return fail(c,METAIL_INVALID_ARGUMENT,"text output required");
    ir_request r={.operation="module_emit",.input=module,.rounds=1};return ir_run(c,&r,NULL,output);
}
metail_status metail_module_linkage(metail_context *c, const metail_module *module, char **output) {
    if (!output) return fail(c,METAIL_INVALID_ARGUMENT,"JSON output required");
    ir_request r={.operation="module_linkage",.input=module,.rounds=1};return ir_run(c,&r,NULL,output);
}
metail_status metail_module_analyze(metail_context *c, const metail_module *module, const char *passes, char **output) {
    if (!output) return fail(c,METAIL_INVALID_ARGUMENT,"JSON output required");
    ir_request r={.operation="module_analyze",.input=module,.passes=passes,.rounds=1};return ir_run(c,&r,NULL,output);
}
metail_status metail_module_optimize(metail_context *c, const metail_module *module, const char *passes,
    int rounds, metail_module **output, char **stats) {
    if (output && (void *)output==(void *)stats) {
        *output=NULL;return fail(c,METAIL_INVALID_ARGUMENT,"distinct output slots required");
    }
    if (!output) { if (stats) *stats=NULL;return fail(c,METAIL_INVALID_ARGUMENT,"module output required"); }
    ir_request r={.operation="module_optimize",.input=module,.passes=passes,.rounds=rounds};
    return ir_run(c,&r,output,stats);
}
