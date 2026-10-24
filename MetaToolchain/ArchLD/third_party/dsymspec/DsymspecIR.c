#include "DsymspecIR.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <inttypes.h>
struct DsymArena {void **alloc;size_t count,cap;};
DsymArena *dsym_arena_new(void){return calloc(1,sizeof(DsymArena));}
void dsym_arena_free(DsymArena*a){if(!a)return;for(size_t i=0;i<a->count;i++)free(a->alloc[i]);free(a->alloc);free(a);}
void *dsym_arena_alloc(DsymArena*a,size_t n){if(!a)return NULL;if(a->count==a->cap){size_t k=a->cap?a->cap*2:32;if(k<a->cap||k>SIZE_MAX/sizeof(void*))return NULL;void **p=realloc(a->alloc,k*sizeof(void*));if(!p)return NULL;a->alloc=p;a->cap=k;}void *p=calloc(1,n?n:1);if(p)a->alloc[a->count++]=p;return p;}
void *dsym_arena_calloc(DsymArena*a,size_t n,size_t s){if(s&&n>SIZE_MAX/s)return NULL;size_t t=n*s;void*p=dsym_arena_alloc(a,t);if(p)memset(p,0,t);return p;}
char *dsym_arena_strndup(DsymArena*a,const char*s,size_t n){if(n==SIZE_MAX)return NULL;char*p=dsym_arena_alloc(a,n+1);if(p){memcpy(p,s,n);p[n]=0;}return p;}
char *dsym_arena_strdup(DsymArena*a,const char*s){return dsym_arena_strndup(a,s,strlen(s));}
char *dsym_arena_printf(DsymArena*a,const char*fmt,...){va_list v;va_start(v,fmt);va_list q;va_copy(q,v);int n=vsnprintf(NULL,0,fmt,v);va_end(v);if(n<0){va_end(q);return NULL;}char*p=dsym_arena_alloc(a,(size_t)n+1);if(p)vsnprintf(p,(size_t)n+1,fmt,q);va_end(q);return p;}
void dsym_diags_init(DsymDiags*d){memset(d,0,sizeof(*d));}
void dsym_diags_free(DsymDiags*d){for(size_t i=0;i<d->count;i++){free((void*)d->items[i].file);free((void*)d->items[i].path);free((void*)d->items[i].msg);}free(d->items);memset(d,0,sizeof(*d));}
void dsym_diag_add(DsymDiags*d,int sev,const char*file,unsigned line,unsigned col,const char*path,const char*fmt,...){if(!d)return; if(sev==DSYM_ERROR)d->errors++;else d->warnings++;if(d->count==d->cap){size_t cap=d->cap?d->cap*2:8;DsymDiag*p=realloc(d->items,cap*sizeof(*p));if(!p)return;d->items=p;d->cap=cap;}DsymDiag *x=&d->items[d->count++];memset(x,0,sizeof(*x));x->severity=sev;x->line=line;x->col=col;x->file=file?strdup(file):NULL;x->path=path?strdup(path):NULL;va_list a;va_start(a,fmt);va_list b;va_copy(b,a);int n=vsnprintf(NULL,0,fmt,a);va_end(a);if(n>=0){char*p=malloc((size_t)n+1);if(p){vsnprintf(p,(size_t)n+1,fmt,b);x->msg=p;}}va_end(b);}
void dsym_buf_init(DsymBuf*b){memset(b,0,sizeof(*b));}
void dsym_buf_free(DsymBuf*b){free(b->data);memset(b,0,sizeof(*b));}
static bool append(DsymBuf*b,const char*s,size_t n){if(n>SIZE_MAX-b->len-1)return false;size_t need=b->len+n+1;if(need>b->cap){size_t c=b->cap?b->cap:256;while(c<need){if(c>SIZE_MAX/2)return false;c*=2;}char*p=realloc(b->data,c);if(!p)return false;b->data=p;b->cap=c;}memcpy(b->data+b->len,s,n);b->len+=n;b->data[b->len]=0;return true;}
bool dsym_buf_puts(DsymBuf*b,const char*s){return append(b,s,strlen(s));}
bool dsym_buf_read_file(DsymBuf*b,const char*name){FILE*f=fopen(name,"rb");if(!f)return false;char buf[8192];size_t n;while((n=fread(buf,1,sizeof(buf),f))>0)if(!append(b,buf,n)){fclose(f);return false;}bool ok=!ferror(f);fclose(f);return ok;}
DsymFormat dsym_format_from_path(const char*p){const char *dot=strrchr(p,'.');if(!dot)return DSYM_FORMAT_AUTO;if(!strcmp(dot,".json"))return DSYM_FORMAT_JSON;if(!strcmp(dot,".yaml")||!strcmp(dot,".yml"))return DSYM_FORMAT_YAML;if(!strcmp(dot,".xml"))return DSYM_FORMAT_XML;if(!strcmp(dot,".sexp"))return DSYM_FORMAT_SEXP;return DSYM_FORMAT_AUTO;}
static DsymNode *node(DsymArena*a,DsymNodeKind kind){DsymNode*n=dsym_arena_calloc(a,1,sizeof(*n));if(n)n->kind=kind;return n;}
DsymNode *dsym_node_map(DsymArena*a){return node(a,DSYM_NODE_MAP);}
DsymNode *dsym_node_seq(DsymArena*a){return node(a,DSYM_NODE_SEQ);}
DsymNode *dsym_node_int(DsymArena*a,int64_t i){DsymNode*n=node(a,DSYM_NODE_INT);if(n)n->v.i=i;return n;}
DsymNode *dsym_node_uint(DsymArena*a,uint64_t u){DsymNode*n=node(a,DSYM_NODE_UINT);if(n)n->v.u=u;return n;}
DsymNode *dsym_node_bool(DsymArena*a,bool b){DsymNode*n=node(a,DSYM_NODE_BOOL);if(n)n->v.b=b;return n;}
DsymNode *dsym_node_str_ref(DsymArena*a,const char*s){DsymNode*n=node(a,DSYM_NODE_STR);if(n){n->v.str.ptr=dsym_arena_strdup(a,s);n->v.str.len=strlen(s);}return n;}
bool dsym_map_put_ref(DsymArena*a,DsymNode*n,const char*key,DsymNode*v){if(!n||n->kind!=DSYM_NODE_MAP)return false;if(n->v.map.count==n->v.map.cap){size_t cap=n->v.map.cap?n->v.map.cap*2:8;DsymMapPair*p=dsym_arena_calloc(a,cap,sizeof(*p));if(!p)return false;if(n->v.map.items)memcpy(p,n->v.map.items,n->v.map.count*sizeof(*p));n->v.map.items=p;n->v.map.cap=cap;}n->v.map.items[n->v.map.count++]=(DsymMapPair){dsym_arena_strdup(a,key),v};return true;}
bool dsym_seq_push(DsymArena*a,DsymNode*n,DsymNode*v){if(!n||n->kind!=DSYM_NODE_SEQ)return false;if(n->v.seq.count==n->v.seq.cap){size_t cap=n->v.seq.cap?n->v.seq.cap*2:8;DsymNode**p=dsym_arena_calloc(a,cap,sizeof(*p));if(!p)return false;if(n->v.seq.items)memcpy(p,n->v.seq.items,n->v.seq.count*sizeof(*p));n->v.seq.items=p;n->v.seq.cap=cap;}n->v.seq.items[n->v.seq.count++]=v;return true;}
const DsymNode *dsym_map_get(const DsymNode*n,const char*k){if(!n||n->kind!=DSYM_NODE_MAP)return NULL;for(size_t i=0;i<n->v.map.count;i++)if(!strcmp(n->v.map.items[i].key,k))return n->v.map.items[i].value;return NULL;}
/* Bounded recursive JSON parser; source data converted from the supplied YAML. */
typedef struct {const char*p,*end;int depth;DsymArena*a;DsymDiags*d;const char*file;} Parser;
static void ws(Parser*p){while(p->p<p->end && (unsigned char)*p->p<=32)p->p++;}
static bool hex(char c,int*v){if(c>='0'&&c<='9'){*v=c-'0';return true;}if(c>='a'&&c<='f'){*v=c-'a'+10;return true;}if(c>='A'&&c<='F'){*v=c-'A'+10;return true;}return false;}
static char *jstr(Parser*p){if(p->p>=p->end||*p->p!='"')return NULL;p->p++;DsymBuf b;dsym_buf_init(&b);while(p->p<p->end){unsigned char c=(unsigned char)*p->p++;if(c=='"'){char*str=dsym_arena_strndup(p->a,b.data?b.data:"",b.len);dsym_buf_free(&b);return str;}if(c<32)break;if(c=='\\'){if(p->p>=p->end)break;char e=*p->p++;switch(e){case '"':c='"';break;case '\\':c='\\';break;case '/':c='/';break;case 'n':c='\n';break;case 'r':c='\r';break;case 't':c='\t';break;case 'b':c='\b';break;case 'f':c='\f';break;case 'u':{if(p->end-p->p<4)goto fail;int a,bx,d,f;if(!hex(p->p[0],&a)||!hex(p->p[1],&bx)||!hex(p->p[2],&d)||!hex(p->p[3],&f))goto fail;p->p+=4;unsigned cp=(unsigned)((a<<12)|(bx<<8)|(d<<4)|f);char u[4];size_t n=0;if(cp<128)u[n++]=(char)cp;else if(cp<2048){u[n++]=(char)(0xc0|(cp>>6));u[n++]=(char)(0x80|(cp&63));}else{u[n++]=(char)(0xe0|(cp>>12));u[n++]=(char)(0x80|((cp>>6)&63));u[n++]=(char)(0x80|(cp&63));}if(!append(&b,u,n))goto fail;continue;}default:goto fail;}}
if(!append(&b,(char*)&c,1))goto fail;}fail:dsym_buf_free(&b);return NULL;}
static DsymNode *value(Parser*p){ws(p);if(p->depth++>128)return NULL;DsymNode*n=NULL;if(p->p>=p->end)goto done;
if(*p->p=='{'){p->p++;n=dsym_node_map(p->a);ws(p);if(p->p<p->end&&*p->p=='}'){p->p++;goto done;}for(;;){ws(p);char *k=jstr(p);if(!k)goto error;ws(p);if(p->p>=p->end||*p->p++!=':')goto error;DsymNode*v=value(p);if(!v||!dsym_map_put_ref(p->a,n,k,v))goto error;ws(p);if(p->p>=p->end)goto error;char c=*p->p++;if(c=='}')break;if(c!=',')goto error;}}
else if(*p->p=='['){p->p++;n=dsym_node_seq(p->a);ws(p);if(p->p<p->end&&*p->p==']'){p->p++;goto done;}for(;;){DsymNode*v=value(p);if(!v||!dsym_seq_push(p->a,n,v))goto error;ws(p);if(p->p>=p->end)goto error;char c=*p->p++;if(c==']')break;if(c!=',')goto error;}}
else if(*p->p=='"'){char*s=jstr(p);if(!s)goto error;n=dsym_node_str_ref(p->a,s);}
else if(p->end-p->p>=4&&!memcmp(p->p,"true",4)){p->p+=4;n=dsym_node_bool(p->a,true);}
else if(p->end-p->p>=5&&!memcmp(p->p,"false",5)){p->p+=5;n=dsym_node_bool(p->a,false);}
else if(p->end-p->p>=4&&!memcmp(p->p,"null",4)){p->p+=4;n=node(p->a,DSYM_NODE_NULL);}
else {const char*s=p->p;bool neg=false;if(*p->p=='-'){neg=true;p->p++;}if(p->p==p->end||!isdigit((unsigned char)*p->p))goto error;while(p->p<p->end&&isdigit((unsigned char)*p->p))p->p++;if(p->p<p->end&&(*p->p=='.'||*p->p=='e'||*p->p=='E'))goto error;char *num=dsym_arena_strndup(p->a,s,(size_t)(p->p-s));if(!num)goto error;errno=0;char *end=NULL;if(neg){intmax_t x=strtoimax(num,&end,10);if(errno||*end)goto error;n=dsym_node_int(p->a,(int64_t)x);}else{uintmax_t x=strtoumax(num,&end,10);if(errno||*end)goto error;n=x<=INT64_MAX?dsym_node_int(p->a,(int64_t)x):dsym_node_uint(p->a,(uint64_t)x);}}goto done;
error:n=NULL;
done:p->depth--;return n;}
DsymNode *dsym_tree_read(DsymFormat f,DsymArena*a,const char*buf,size_t len,const char*name,DsymDiags*d){(void)f;Parser p={buf,buf+len,0,a,d,name};DsymNode*n=value(&p);ws(&p);if(!n||p.p!=p.end){dsym_diag_add(d,DSYM_ERROR,name,0,0,NULL,"expected valid JSON description (YAML sources require conversion)");return NULL;}return n;}
static bool quoted(DsymBuf*b,const char*s){if(!dsym_buf_puts(b,"\""))return false;for(;*s;s++){char e[7];if(*s=='"'||*s=='\\'){char z[2]={'\\',*s};if(!append(b,z,2))return false;}else if((unsigned char)*s<32){snprintf(e,sizeof(e),"\\u%04x",(unsigned char)*s);if(!dsym_buf_puts(b,e))return false;}else if(!append(b,s,1))return false;}return dsym_buf_puts(b,"\"");}
static bool write_node(DsymBuf*b,const DsymNode*n,int d){if(d>128)return false;char num[64];switch(n->kind){case DSYM_NODE_MAP:if(!dsym_buf_puts(b,"{"))return false;for(size_t i=0;i<n->v.map.count;i++){if(i&&!dsym_buf_puts(b,","))return false;if(!quoted(b,n->v.map.items[i].key)||!dsym_buf_puts(b,":")||!write_node(b,n->v.map.items[i].value,d+1))return false;}return dsym_buf_puts(b,"}");case DSYM_NODE_SEQ:if(!dsym_buf_puts(b,"["))return false;for(size_t i=0;i<n->v.seq.count;i++){if(i&&!dsym_buf_puts(b,","))return false;if(!write_node(b,n->v.seq.items[i],d+1))return false;}return dsym_buf_puts(b,"]");case DSYM_NODE_STR:return quoted(b,n->v.str.ptr);case DSYM_NODE_INT:snprintf(num,sizeof num,"%"PRId64,n->v.i);return dsym_buf_puts(b,num);case DSYM_NODE_UINT:snprintf(num,sizeof num,"%"PRIu64,n->v.u);return dsym_buf_puts(b,num);case DSYM_NODE_BOOL:return dsym_buf_puts(b,n->v.b?"true":"false");default:return dsym_buf_puts(b,"null");}}
bool dsym_tree_write(DsymFormat f,const DsymNode*n,DsymBuf*b,uint32_t opts,DsymDiags*d){(void)f;(void)opts;(void)d;return write_node(b,n,0)&&dsym_buf_puts(b,"\n");}
