/* Native checked integer evaluation for MetaIL's explicitly signed 64-bit IR.
   Overflow wraps modulo 2^64; division by zero and MIN_INT/-1 are not folded.
   Shift counts outside [0,63] are not folded. */
#define _POSIX_C_SOURCE 200809L
#include "lua.h"
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
_Static_assert(sizeof(lua_Integer)==8, "MetaIL requires 64-bit Lua integers");
static int64_t signed_bits(uint64_t value) {
    return value<=INT64_MAX ? (int64_t)value : -1-(int64_t)(UINT64_MAX-value);
}
static int push_error(lua_State *L, const char *msg) {
    lua_pushnil(L);lua_pushstring(L,msg);return 2;
}
static int op_eval(lua_State *L) {
    size_t op_len=0;
    const char *op=luaL_checklstring(L,1,&op_len);
    if(strlen(op)!=op_len) return push_error(L,"unknown operator");
    if(!lua_isinteger(L,2)) return push_error(L,"non-integer operand");
    int64_t x=(int64_t)luaL_checkinteger(L,2), y=0;
    int unary= !strcmp(op,"neg") || !strcmp(op,"bnot") || !strcmp(op,"lnot");
    if(!unary) {
        if(!lua_isinteger(L,3)) return push_error(L,"non-integer operand");
        y=(int64_t)luaL_checkinteger(L,3);
    }
    uint64_t a=(uint64_t)x, b=(uint64_t)y, z=0;
    if(!strcmp(op,"add")) z=a+b;
    else if(!strcmp(op,"sub")) z=a-b;
    else if(!strcmp(op,"mul")) z=a*b;
    else if(!strcmp(op,"neg")) z=0-a;
    else if(!strcmp(op,"band")) z=a&b;
    else if(!strcmp(op,"bor")) z=a|b;
    else if(!strcmp(op,"bxor")) z=a^b;
    else if(!strcmp(op,"bnot")) z=~a;
    else if(!strcmp(op,"shl") || !strcmp(op,"shr")) {
        if(y<0 || y>63) return push_error(L,"invalid shift count");
        if(!strcmp(op,"shl")) z=a<<(unsigned)y;
        else {
            z=a>>(unsigned)y;
            if(x<0 && y!=0) z|=UINT64_MAX<<(64-(unsigned)y);
        }
    }
    else if(!strcmp(op,"div") || !strcmp(op,"mod")) {
        if(y==0) return push_error(L,"division by zero");
        if(x==INT64_MIN && y==-1) return push_error(L,"division overflow");
        z=(uint64_t)(!strcmp(op,"div") ? x/y : x%y); /* truncation toward zero */
    }
    else if(!strcmp(op,"eq")) z=x==y;
    else if(!strcmp(op,"ne")) z=x!=y;
    else if(!strcmp(op,"lt")) z=x<y;
    else if(!strcmp(op,"le")) z=x<=y;
    else if(!strcmp(op,"gt")) z=x>y;
    else if(!strcmp(op,"ge")) z=x>=y;
    else if(!strcmp(op,"land")) z=(x!=0 && y!=0);
    else if(!strcmp(op,"lor")) z=(x!=0 || y!=0);
    else if(!strcmp(op,"lnot")) z=(x==0);
    else return push_error(L,"unknown operator");
    lua_pushinteger(L,(lua_Integer)signed_bits(z));
    return 1;
}
static int op_bits(lua_State *L){lua_pushinteger(L,64);return 1;}
static int compare_paths(const void *a, const void *b) {
    const char *const *aa=(const char *const *)a, *const *bb=(const char *const *)b;
    return strcmp(*aa,*bb);
}
static int scan_impl(const char *dir, char ***items, size_t *n, size_t *capacity, unsigned depth) {
    if(depth>64) return -1;
    DIR *d=opendir(dir);
    if(!d) return -1;
    struct dirent *entry;
    for(;;) {
        errno=0;
        entry=readdir(d);
        if(!entry) {int failed=errno!=0;closedir(d);return failed ? -1 : 0;}
        if(!strcmp(entry->d_name,".") || !strcmp(entry->d_name,"..")) continue;
        size_t dirlen=strlen(dir), namelen=strlen(entry->d_name);
        if(dirlen>SIZE_MAX-namelen-2) {closedir(d);return -1;}
        size_t len=dirlen+namelen+2;
        char *path=malloc(len);
        if(!path){closedir(d);return -1;}
        snprintf(path,len,"%s/%s",dir,entry->d_name);
        struct stat st;
        if(lstat(path,&st)!=0) {free(path);closedir(d);return -1;}
        if(S_ISDIR(st.st_mode)) {
            int ok=scan_impl(path,items,n,capacity,depth+1);free(path);
            if(ok) {closedir(d);return -1;}
        } else if(S_ISREG(st.st_mode) &&
                  strlen(path)>5 && !strcmp(path+strlen(path)-5,".optk")) {
            if(*n==*capacity) {
                if(*capacity>(SIZE_MAX/sizeof(char *)-32)/2 || *n>=100000) {
                    free(path);closedir(d);return -1;
                }
                size_t c=(*capacity)*2+32;
                char **new_items=realloc(*items,c*sizeof(char *));
                if(!new_items){free(path);closedir(d);return -1;}
                *items=new_items;*capacity=c;
            }
            (*items)[(*n)++]=path;
        } else free(path);
    }
}
static int op_scan(lua_State *L) {
    size_t len=0;
    const char *directory=luaL_checklstring(L,1,&len);
    if(strlen(directory)!=len) return luaL_error(L,"invalid directory path");
    char **items=NULL;size_t n=0,capacity=0;
    int ok=scan_impl(directory,&items,&n,&capacity,0);
    if(ok) {
        for(size_t i=0;i<n;i++) free(items[i]);
        free(items);
        return luaL_error(L,"cannot scan kernel directory: %s",directory);
    }
    if(n>1) qsort(items,n,sizeof(char *),compare_paths);
    lua_createtable(L,(int)n,0);
    for(size_t i=0;i<n;i++) {
        lua_pushstring(L,items[i]);lua_rawseti(L,-2,(lua_Integer)i+1);free(items[i]);
    }
    free(items);return 1;
}
void lkopt_register(lua_State *L) {
    lua_createtable(L,0,3);
    lua_pushcclosure(L,op_eval,0);lua_setfield(L,-2,"eval");
    lua_pushcclosure(L,op_bits,0);lua_setfield(L,-2,"bits");
    lua_pushcclosure(L,op_scan,0);lua_setfield(L,-2,"scan");
    lua_setglobal(L,"lkopt_native");
}
