/** \file GPPX.c
 * Freestanding C extension utilities for the C++17 GPPX engine.
 * Unlike the former placeholder, this translation unit is linked into libgppx.
 * This module intentionally uses only ISO C11 and the public C engine API.
 */
#include "GPPX.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int ascii_first(unsigned char x) {
    return (x >= 'A' && x <= 'Z') || (x >= 'a' && x <= 'z') || x == '_';
}
static int ascii_next(unsigned char x) {
    return ascii_first(x) || (x >= '0' && x <= '9');
}
static int safe_mul(size_t n,size_t k,size_t extra,size_t *out) {
    if (k && n > (SIZE_MAX-extra)/k) return 1;
    *out = n*k+extra; return 0;
}
static char *copy_range(const char *s,size_t n) {
    char *p;
    if (n == SIZE_MAX || !(p=(char*)malloc(n+1))) return NULL;
    memcpy(p,s,n); p[n]='\0';return p;
}
int gppx_directive_valid_name(const char *s) {
    if (!s || !ascii_first((unsigned char)*s)) return 0;
    for (++s;*s;++s) if (!ascii_next((unsigned char)*s)) return 0;
    return 1;
}
int gppx_directive_split(const char *text,char **name,char **arguments) {
    const char *a,*b,*end;
    if (!name || !arguments || !text || name==arguments) return 1;
    *name=NULL;*arguments=NULL;
    a=text;while (*a && isspace((unsigned char)*a)) ++a;
    if (!ascii_first((unsigned char)*a)) return 1;
    b=a+1;while (ascii_next((unsigned char)*b)) ++b;
    if (*b && !isspace((unsigned char)*b)) return 1;
    *name=copy_range(a,(size_t)(b-a));if (!*name) return 1;
    while (*b && isspace((unsigned char)*b)) ++b;
    end=b+strlen(b);while (end>b && isspace((unsigned char)end[-1])) --end;
    *arguments=copy_range(b,(size_t)(end-b));
    if (!*arguments) {free(*name);*name=NULL;return 1;}
    return 0;
}
int gppx_macro_define_spec(GPPX_State *state,const char *spec) {
    const char *eq;
    char *name;
    int rc;
    if (!state || !spec) return 1;
    eq=strchr(spec,'=');
    name=copy_range(spec,eq?(size_t)(eq-spec):strlen(spec));
    if (!name) return 1;
    if (!gppx_directive_valid_name(name)) {free(name);return 1;}
    rc=gppx_define(state,name,eq?eq+1:"1");free(name);return rc;
}
int gppx_macro_process_text(GPPX_State *state,const char *text,char **result) {
    return gppx_process(state,text,"<macro-text>",result);
}
int gppx_syntax_apply_profile(GPPX_State *state,const char *profile) {
    if (!profile) return 1;
    return gppx_set_option(state,"profile",profile);
}
int gppx_syntax_delimiters(GPPX_State *state,const char *dir,const char *open,const char *close) {
    if (!state) return 1;
    if (dir && gppx_set_option(state,"directive",dir)) return 1;
    if (open && gppx_set_option(state,"macro_open",open)) return 1;
    if (close && gppx_set_option(state,"macro_close",close)) return 1;
    return 0;
}
int gppx_reader_load_bytes(const char *path,unsigned char **data,size_t *size) {
    unsigned char *out=NULL;
    FILE *f;
    size_t len=0,cap=4096;
    if (!path || !data || !size) return 1;
    *data=NULL;*size=0;
    if (!(f=fopen(path,"rb"))) return 1;
    out=(unsigned char*)malloc(cap+1);
    if (!out) {fclose(f);return 1;}
    for (;;) {
        size_t n;
        if (len==cap) {
            unsigned char *grown;
            if (cap > (SIZE_MAX-1)/2) goto fail;
            cap*=2;grown=(unsigned char*)realloc(out,cap+1);
            if (!grown) goto fail;
            out=grown;
        }
        n=fread(out+len,1,cap-len,f);len+=n;
        if (n==0) {
            if (ferror(f)) goto fail;
            if (feof(f)) break;
        }
    }
    if (fclose(f)) {free(out);return 1;}
    out[len]=0;*data=out;*size=len;return 0;
fail:
    fclose(f);free(out);return 1;
}
int gppx_sink_write_file(const char *path,const void *bytes,size_t size,int append) {
    FILE *f;
    int failed=0;
    if (!path || (!bytes && size)) return 1;
    f=fopen(path,append?"ab":"wb");if (!f) return 1;
    if (size && fwrite(bytes,1,size,f)!=size) failed=1;
    if (fclose(f)!=0) failed=1;
    return failed;
}
int gppx_sink_file_callback(GPPX_State *state,const char *destination,const char *data,
                            size_t size,void *userdata) {
    const GPPX_FileSinkConfig *conf=(const GPPX_FileSinkConfig*)userdata;
    (void)state;
    return gppx_sink_write_file(conf&&conf->path?conf->path:destination,
                                data,size,conf?conf->append:0);
}
static const char HEX[]="0123456789ABCDEF";
static int encode_hex(const unsigned char *data,size_t n,char **result,int commas) {
    size_t cap,i,pos=0;
    char *out;
    if (!result) return 1;
    *result=NULL;
    if (!data && n) return 1;
    if (safe_mul(n,commas?6:2,1,&cap)) return 1;
    /* C-array literals: 0xFF, 0xAB (last literal occupies 4 bytes). */
    if (commas && n) cap-=2;
    if (!(out=(char*)malloc(cap))) return 1;
    for (i=0;i<n;++i) {
        if (commas) {
            if (i) {out[pos++]=',';out[pos++]=' ';}
            out[pos++]='0';out[pos++]='x';
        }
        out[pos++]=HEX[data[i]>>4];out[pos++]=HEX[data[i]&15];
    }
    out[pos]='\0';*result=out;return 0;
}
int gppx_embed_hex(const unsigned char *data,size_t size,char **result) {
    return encode_hex(data,size,result,0);
}
int gppx_embed_c_array(const unsigned char *data,size_t size,char **result) {
    return encode_hex(data,size,result,1);
}
int gppx_embed_base64(const unsigned char *data,size_t size,char **result) {
    static const char abc[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t groups,len,i,pos=0;
    char *out;
    if (!result) return 1;
    *result=NULL;
    if (!data && size) return 1;
    if (size > SIZE_MAX-2) return 1;
    groups=(size+2)/3;
    if (safe_mul(groups,4,1,&len)) return 1;
    if (!(out=(char*)malloc(len))) return 1;
    for (i=0;i<size;i+=3) {
        unsigned a=data[i],b=i+1<size?data[i+1]:0,c=i+2<size?data[i+2]:0;
        out[pos++]=abc[a>>2];out[pos++]=abc[((a&3)<<4)|(b>>4)];
        out[pos++]=i+1<size?abc[((b&15)<<2)|(c>>6)]:'=';
        out[pos++]=i+2<size?abc[c&63]:'=';
    }
    out[pos]='\0';*result=out;return 0;
}
int gppx_embed_hex_callback(GPPX_State *s,const unsigned char *b,size_t n,char **out,void *unused) {
    (void)s;(void)unused;return gppx_embed_hex(b,n,out);
}
int gppx_embed_array_callback(GPPX_State *s,const unsigned char *b,size_t n,char **out,void *unused) {
    (void)s;(void)unused;return gppx_embed_c_array(b,n,out);
}
int gppx_embed_base64_callback(GPPX_State *s,const unsigned char *b,size_t n,char **out,void *unused) {
    (void)s;(void)unused;return gppx_embed_base64(b,n,out);
}
int gppx_rewriter_replace_literal(const char *src,const char *needle,
                                  const char *repl,char **result) {
    size_t sn,nn,rn,count=0,newlen,pos=0;
    const char *p,*found;
    char *out;
    if (!result) return 1;
    *result=NULL;
    if (!src || !needle || !repl || !*needle) return 1;
    sn=strlen(src);nn=strlen(needle);rn=strlen(repl);
    for (p=src;(found=strstr(p,needle))!=NULL;p=found+nn) ++count;
    newlen=sn;
    if (rn>nn) {
        size_t extra;
        if (safe_mul(count,rn-nn,0,&extra) || newlen>SIZE_MAX-extra) return 1;
        newlen+=extra;
    } else newlen-=count*(nn-rn);
    if (newlen==SIZE_MAX || !(out=(char*)malloc(newlen+1))) return 1;
    for (p=src;(found=strstr(p,needle))!=NULL;p=found+nn) {
        size_t chunk=(size_t)(found-p);
        memcpy(out+pos,p,chunk);pos+=chunk;
        memcpy(out+pos,repl,rn);pos+=rn;
    }
    memcpy(out+pos,p,strlen(p)+1);
    *result=out;return 0;
}
int gppx_rewriter_literal_callback(GPPX_State *state,const char *src,char **out,void *userdata) {
    const GPPX_LiteralRewriter *conf=(const GPPX_LiteralRewriter*)userdata;
    (void)state;
    if (!conf) return 1;
    return gppx_rewriter_replace_literal(src,conf->needle,conf->replacement,out);
}
int gppx_pmatch_prefix(const char *pattern,const char *subject) {
    size_t n;
    if (!pattern || !subject) return -1;
    n=strlen(pattern);return strncmp(pattern,subject,n)==0;
}
/* Constant-memory glob matcher, with explicit backtracking only at `*`.
 * Escapes are handled while walking the pattern; the algorithm does not build
 * a regex or recurse on attacker-controlled data.
 */
int gppx_pmatch_glob(const char *pattern,const char *subject) {
    const char *p=pattern,*s=subject,*afterstar=NULL,*retry=NULL;
    if (!p || !s) return -1;
    while (*s) {
        if (*p=='*') {while (*p=='*') ++p;afterstar=p;retry=s;continue;}
        if (*p) {
            const char *next=p+1;
            char token=*p;
            int wildcard=(token=='?');
            if (token=='\\' && *next) {token=*next++;wildcard=0;}
            if (wildcard || token==*s) {p=next;++s;continue;}
        }
        if (!afterstar) return 0;
        ++retry;s=retry;p=afterstar;
    }
    while (*p=='*') ++p;
    return *p=='\0';
}
int gppx_pmatch_glob_callback(GPPX_State *s,const char *pat,const char *txt,void *unused) {
    (void)s;(void)unused;return gppx_pmatch_glob(pat,txt);
}
int gppx_pmatch_prefix_callback(GPPX_State *s,const char *pat,const char *txt,void *unused) {
    (void)s;(void)unused;return gppx_pmatch_prefix(pat,txt);
}

int gppx_style_apply(GPPX_State *state,const GPPX_Style *style) {
    if (!state || !style || style->expand_strings < -1 || style->expand_strings > 1) return 1;
    if (style->macro_style && strcmp(style->macro_style,"bare") &&
        strcmp(style->macro_style,"dollar") && strcmp(style->macro_style,"mustache")) return 1;
    if (style->macro_style && gppx_set_option(state,"macro_style",style->macro_style)) return 1;
    if (style->line_comment && gppx_set_option(state,"line_comment",style->line_comment)) return 1;
    if (style->block_open && gppx_set_option(state,"block_open",style->block_open)) return 1;
    if (style->block_close && gppx_set_option(state,"block_close",style->block_close)) return 1;
    return style->expand_strings == -1 ? 0 :
        gppx_set_option(state,"expand_strings",style->expand_strings ? "true" : "false");
}

/* Two passes avoid overallocating tab_width * input_length. */
static int layout_pass(const char *text,const GPPX_Layout *layout,char *out,size_t *length) {
    size_t pos=0,col=0;
    int start=1;
    for (;*text;++text) {
        size_t n=1;
        char c=*text;
        if (c=='\r' || c=='\n') {
            if (c=='\r' && text[1]=='\n') ++text;
            c='\n'; col=0;start=1;
        } else {
            if (start) {
                n=layout->indent_spaces;
                if (n>SIZE_MAX-pos) return 1;
                if (out) memset(out+pos,' ',n);
                pos+=n;start=0;
                /* Tab stops are relative to the original line, before prefixing. */
            }
            n=(c=='\t' && layout->tab_width) ? layout->tab_width-col%layout->tab_width : 1;
            if (n>SIZE_MAX-col) return 1;
            col+=n;
        }
        if (n>SIZE_MAX-pos) return 1;
        if (out) {
            if (c=='\t' && layout->tab_width) memset(out+pos,' ',n);
            else out[pos]=c;
        }
        pos+=n;
    }
    *length=pos;return 0;
}
int gppx_layout_format(const char *text,const GPPX_Layout *layout,char **output) {
    size_t n;
    char *out;
    if (!output) return 1;
    *output=NULL;
    if (!text || !layout || layout_pass(text,layout,NULL,&n) || n==SIZE_MAX) return 1;
    out=(char*)malloc(n+1);if (!out) return 1;
    if (layout_pass(text,layout,out,&n)) {free(out);return 1;}
    out[n]=0;*output=out;return 0;
}
int gppx_layout_callback(GPPX_State *state,const char *text,char **output,void *userdata) {
    (void)state;
    return gppx_layout_format(text,(const GPPX_Layout*)userdata,output);
}
int gppx_utf8_next(const unsigned char *bytes,size_t size,size_t *offset,uint32_t *scalar) {
    size_t i,n,j;
    uint32_t cp,min;
    unsigned char c;
    if (!bytes || !offset || !scalar || *offset>=size) return 1;
    i=*offset;c=bytes[i];
    if (c<0x80) {n=1;cp=c;min=0;}
    else if (c>=0xC2 && c<=0xDF) {n=2;cp=c&0x1F;min=0x80;}
    else if (c>=0xE0 && c<=0xEF) {n=3;cp=c&0x0F;min=0x800;}
    else if (c>=0xF0 && c<=0xF4) {n=4;cp=c&7;min=0x10000;}
    else return 1;
    if (n>size-i) return 1;
    for (j=1;j<n;++j) {
        c=bytes[i+j];if ((c&0xC0)!=0x80) return 1;
        cp=(cp<<6)|(c&0x3F);
    }
    if (cp<min || cp>0x10FFFF || (cp>=0xD800 && cp<=0xDFFF)) return 1;
    *scalar=cp;*offset=i+n;return 0;
}
int gppx_utf8_validate(const unsigned char *bytes,size_t size,size_t *error_offset) {
    size_t offset=0;
    uint32_t cp;
    if (error_offset) *error_offset=0;
    if (!bytes && size) return 1;
    while (offset<size) {
        if (gppx_utf8_next(bytes,size,&offset,&cp)) {
            if (error_offset) *error_offset=offset;
            return 1;
        }
    }
    if (error_offset) *error_offset=size;
    return 0;
}
int gppx_utf8_load(const char *path,char **output) {
    unsigned char *bytes;
    size_t size,start=0;
    if (!output) return 1;
    *output=NULL;
    if (gppx_reader_load_bytes(path,&bytes,&size)) return 1;
    if (gppx_utf8_validate(bytes,size,NULL) || memchr(bytes,0,size)) {free(bytes);return 1;}
    if (size>=3 && bytes[0]==0xEF && bytes[1]==0xBB && bytes[2]==0xBF) start=3;
    if (start) memmove(bytes,bytes+start,size-start+1);
    *output=(char*)bytes;return 0;
}
int gppx_utf8_reader_callback(GPPX_State *state,const char *path,char **output,void *userdata) {
    (void)state;(void)userdata;return gppx_utf8_load(path,output);
}
int gppx_metasym_name(const char *name_space,const char *name,char **output) {
    size_t a,b;
    char *out;
    if (!output) return 1;
    *output=NULL;
    if (!gppx_directive_valid_name(name)) return 1;
    if (!name_space || !*name_space) {
        *output=gppx_strdup(name);return *output ? 0 : 1;
    }
    if (!gppx_directive_valid_name(name_space)) return 1;
    a=strlen(name_space);b=strlen(name);
    if (a>SIZE_MAX-2 || b>SIZE_MAX-a-2) return 1;
    out=(char*)malloc(a+b+2);if (!out) return 1;
    memcpy(out,name_space,a);out[a]='_';memcpy(out+a+1,name,b+1);
    *output=out;return 0;
}
int gppx_metasym_define(GPPX_State *state,const char *name_space,const char *name,const char *replacement) {
    char *symbol;
    int rc;
    if (!state || !replacement || gppx_metasym_name(name_space,name,&symbol)) return 1;
    rc=gppx_define(state,symbol,replacement);free(symbol);return rc;
}
int gppx_metasym_undef(GPPX_State *state,const char *name_space,const char *name) {
    char *symbol;
    int rc;
    if (!state || gppx_metasym_name(name_space,name,&symbol)) return 1;
    rc=gppx_undef(state,symbol);free(symbol);return rc;
}
