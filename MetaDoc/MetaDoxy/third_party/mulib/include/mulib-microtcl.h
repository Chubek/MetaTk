/**
 * @file mulib-microtcl.h
 * @brief An embeddable Tcl-inspired command interpreter with a documented small builtin set.
 * @defgroup mulib_microtcl MuLIB microtcl
 * @{
 * @details This interpreter supports a small, deliberately documented Tcl-like subset: `set`, `unset`, `append`, `incr`, arithmetic `expr a + b`, and `puts`. It recognizes semicolons/newlines, whitespace, quoted/braced words, escapes, and `$name` substitution. It does not implement the full Tcl grammar, procedures, Tcl library loading, namespaces, command substitution, or safe sandboxing. `loadall()` creates the interpreter with all *MuLIB-provided* commands enabled.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_MICROTCL before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_MICROTCL_H
#define MULIB_MICROTCL_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <inttypes.h>
#ifdef __cplusplus
extern "C" {
#endif

/** @brief An embeddable Tcl-inspired command interpreter with a documented small builtin set. */
typedef struct mulib_microtcl_var { char *name,*value; struct mulib_microtcl_var *next; } mulib_microtcl_var_t;
/** @brief An embeddable Tcl-inspired command interpreter with a documented small builtin set. */
typedef struct mulib_microtcl { mulib_microtcl_var_t *vars; char *result; int errors; } mulib_microtcl_t;
/** @brief An embeddable Tcl-inspired command interpreter with a documented small builtin set. */
typedef mulib_microtcl_t mulib_micrtcl_t;
/** @brief mulib microtcl loadall; see manual/mulib-microtcl.md for contracts and examples. */
mulib_microtcl_t *mulib_microtcl_loadall(void);
/** @brief mulib microtcl eval; see manual/mulib-microtcl.md for contracts and examples. */
int mulib_microtcl_eval(mulib_microtcl_t*t,const char*script);
/** @brief mulib microtcl result; see manual/mulib-microtcl.md for contracts and examples. */
const char *mulib_microtcl_result(const mulib_microtcl_t*t);
/** @brief mulib microtcl get; see manual/mulib-microtcl.md for contracts and examples. */
const char *mulib_microtcl_get(const mulib_microtcl_t*t,const char*name);
/** @brief mulib microtcl destroy; see manual/mulib-microtcl.md for contracts and examples. */
void mulib_microtcl_destroy(mulib_microtcl_t*t);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_MICROTCL_H */

#if defined(MULIB_IMPL_MICROTCL) && !defined(MULIB_MICROTCL_IMPLEMENTED)
#define MULIB_MICROTCL_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
static char*mulib_microtcl_dup_(const char*s){size_t n=strlen(s);char*p=malloc(n+1);if(p)memcpy(p,s,n+1);return p;}
static int mulib_microtcl_result_(mulib_microtcl_t*t,const char*s){char*p=mulib_microtcl_dup_(s?s:"");if(!p)return 0;free(t->result);t->result=p;return 1;}
mulib_microtcl_t *mulib_microtcl_loadall(void){mulib_microtcl_t*t=calloc(1,sizeof(*t));if(t&&!mulib_microtcl_result_(t,"")){free(t);return NULL;}return t;}
const char *mulib_microtcl_result(const mulib_microtcl_t*t){return t?t->result:NULL;}
const char *mulib_microtcl_get(const mulib_microtcl_t*t,const char*k){if(!t||!k)return NULL;for(mulib_microtcl_var_t*v=t->vars;v;v=v->next)if(!strcmp(v->name,k))return v->value;return NULL;}
static int mulib_microtcl_set_(mulib_microtcl_t*t,const char*k,const char*s){for(mulib_microtcl_var_t*v=t->vars;v;v=v->next)if(!strcmp(v->name,k)){char*p=mulib_microtcl_dup_(s);if(!p)return 0;free(v->value);v->value=p;return 1;}mulib_microtcl_var_t*v=calloc(1,sizeof(*v));if(!v)return 0;v->name=mulib_microtcl_dup_(k);v->value=mulib_microtcl_dup_(s);if(!v->name||!v->value){free(v->name);free(v->value);free(v);return 0;}v->next=t->vars;t->vars=v;return 1;}
void mulib_microtcl_destroy(mulib_microtcl_t*t){if(!t)return;mulib_microtcl_var_t*v=t->vars;while(v){mulib_microtcl_var_t*n=v->next;free(v->name);free(v->value);free(v);v=n;}free(t->result);free(t);}
static void mulib_microtcl_words_free_(char**words,size_t n){for(size_t i=0;i<n;i++)free(words[i]);}
static int mulib_microtcl_exec_(mulib_microtcl_t*t,char**w,size_t n){if(!n)return 1;if(!strcmp(w[0],"set")&&n==3){if(!mulib_microtcl_set_(t,w[1],w[2]))return 0;return mulib_microtcl_result_(t,w[2]);}if(!strcmp(w[0],"set")&&n==2){const char*s=mulib_microtcl_get(t,w[1]);return s?mulib_microtcl_result_(t,s):0;}
if(!strcmp(w[0],"unset")&&n==2){mulib_microtcl_var_t**p=&t->vars;while(*p&&strcmp((*p)->name,w[1]))p=&(*p)->next;if(*p){mulib_microtcl_var_t*v=*p;*p=v->next;free(v->name);free(v->value);free(v);}return mulib_microtcl_result_(t,"");}
if(!strcmp(w[0],"append")&&n==3){const char*old=mulib_microtcl_get(t,w[1]);size_t a=old?strlen(old):0,b=strlen(w[2]);if(b>SIZE_MAX-a-1)return 0;char*s=malloc(a+b+1);if(!s)return 0;if(a)memcpy(s,old,a);memcpy(s+a,w[2],b+1);int ok=mulib_microtcl_set_(t,w[1],s)&&mulib_microtcl_result_(t,s);free(s);return ok;}
if(!strcmp(w[0],"incr")&&(n==2||n==3)){const char*old=mulib_microtcl_get(t,w[1]);char*end=NULL;errno=0;long x=old?strtol(old,&end,10):0;if((old&&(!end||*end))||errno==ERANGE)return 0;errno=0;long inc=n==3?strtol(w[2],&end,10):1;if((n==3&&(!end||*end))||errno==ERANGE)return 0;if((inc>0&&x>LONG_MAX-inc)||(inc<0&&x<LONG_MIN-inc))return 0;char b[64];snprintf(b,sizeof(b),"%ld",x+inc);return mulib_microtcl_set_(t,w[1],b)&&mulib_microtcl_result_(t,b);}
if(!strcmp(w[0],"expr")&&n==4){char*a=NULL,*b=NULL;errno=0;double x=strtod(w[1],&a),y=strtod(w[3],&b);if(errno||!*w[1]||!*w[3]||*a||*b||strlen(w[2])!=1)return 0;double z;switch(*w[2]){case '+':z=x+y;break;case '-':z=x-y;break;case '*':z=x*y;break;case '/':if(y==0)return 0;z=x/y;break;default:return 0;}char res[128];snprintf(res,sizeof(res),"%.17g",z);return mulib_microtcl_result_(t,res);}
if(!strcmp(w[0],"puts")&&n==2){puts(w[1]);return mulib_microtcl_result_(t,w[1]);}
return 0;}
int mulib_microtcl_eval(mulib_microtcl_t*t,const char*script){if(!t||!script)return 0;const char*p=script;while(*p){char*words[64];size_t n=0;while(*p&&*p!='\n'&&*p!=';'){while(*p==' '||*p=='\t'||*p=='\r')p++;if(!*p||*p=='\n'||*p==';')break;if(n>=64){t->errors++;return 0;}char quote=0;if(*p=='"'||*p=='{')quote=*p++;size_t max=strlen(p)+1;char*buf=malloc(max);if(!buf){mulib_microtcl_words_free_(words,n);return 0;}size_t len=0;int closed=quote?0:1;while(*p){if(quote){if(*p==(quote=='{'?'}':'"')){p++;closed=1;break;}}else if(isspace((unsigned char)*p)||*p==';')break;char c=*p++;if(c=='\\'&&*p){c=*p++;if(c=='n')c='\n';else if(c=='t')c='\t';}if(c=='$'&&quote!='{'){const char*start=p;while(isalnum((unsigned char)*p)||*p=='_')p++;if(p>start){size_t sz=(size_t)(p-start);char name[128];if(sz>=sizeof(name)){free(buf);mulib_microtcl_words_free_(words,n);return 0;}memcpy(name,start,sz);name[sz]=0;const char*v=mulib_microtcl_get(t,name);if(!v){free(buf);mulib_microtcl_words_free_(words,n);return 0;}size_t vn=strlen(v);if(len+vn>=max){char*q=realloc(buf,len+vn+strlen(p)+2);if(!q){free(buf);mulib_microtcl_words_free_(words,n);return 0;}buf=q;max=len+vn+strlen(p)+2;}memcpy(buf+len,v,vn);len+=vn;continue;}}buf[len++]=c;}if(!closed){free(buf);mulib_microtcl_words_free_(words,n);t->errors++;return 0;}buf[len]=0;words[n++]=buf;}
int ok=mulib_microtcl_exec_(t,words,n);mulib_microtcl_words_free_(words,n);if(!ok){t->errors++;return 0;}if(*p=='\n'||*p==';')p++;}return 1;}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
