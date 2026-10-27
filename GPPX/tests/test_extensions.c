/* Compile as ISO C11: prove category-specific API is usable from pure C. */
#include "GPPX.h"
#include "GPPX-all.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int count=0;
#define REQUIRE(expr) do {if (!(expr)) {fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#expr);return 1;}++count;} while (0)
static int equals(char *got,const char *expected) {
    int equal=got && strcmp(got,expected)==0;
    if (!equal) fprintf(stderr,"expected [%s], got [%s]\n",expected,got?got:"<null>");
    gppx_free(got);return equal;
}
int main(void) {
    GPPX_State *state=gppx_create();
    char *name=NULL,*args=NULL,*out=NULL;
    const unsigned char blob[3]={'A',0,'B'};
    unsigned char *readback=NULL;
    size_t size=0;
    GPPX_LiteralRewriter replace={"pink","blue"};
    GPPX_FileSinkConfig sink={GPPX_TEST_DATA_PATH,0};
    REQUIRE(state!=NULL);
    REQUIRE(gppx_directive_valid_name("my_directive"));
    REQUIRE(!gppx_directive_valid_name("2bad"));
    REQUIRE(gppx_directive_split("  define   X  1  ",&name,&args)==0);
    REQUIRE(strcmp(name,"define")==0 && strcmp(args,"X  1")==0);
    gppx_free(name);gppx_free(args);
    REQUIRE(gppx_directive_split("@bad",&name,&args)!=0 && !name && !args);
    REQUIRE(gppx_macro_define_spec(state,"THING=loaded")==0);
    REQUIRE(gppx_macro_process_text(state,"THING\n",&out)==0);
    REQUIRE(equals(out,"loaded\n"));
    REQUIRE(gppx_macro_define_spec(state,"DEFAULT")==0);
    REQUIRE(gppx_macro_process_text(state,"DEFAULT\n",&out)==0);
    REQUIRE(equals(out,"1\n"));
    REQUIRE(gppx_macro_define_spec(state,"9NOTVALID")!=0);
    REQUIRE(gppx_syntax_apply_profile(state,"percent")==0);
    REQUIRE(gppx_process(state,"%define X yes\nX\n","<test>",&out)==0);
    REQUIRE(equals(out,"yes\n"));
    REQUIRE(gppx_syntax_delimiters(state,"#","$(", ")")==0);
    REQUIRE(gppx_process(state,"#define Y ok\n$(Y)\n","<test>",&out)==0);
    REQUIRE(equals(out,"ok\n"));
    REQUIRE(gppx_embed_hex(blob,3,&out)==0);
    REQUIRE(equals(out,"410042"));
    REQUIRE(gppx_embed_c_array(blob,3,&out)==0);
    REQUIRE(equals(out,"0x41, 0x00, 0x42"));
    REQUIRE(gppx_embed_base64(blob,3,&out)==0);
    REQUIRE(equals(out,"QQBC"));
    REQUIRE(gppx_embed_base64(NULL,0,&out)==0);
    REQUIRE(equals(out,""));
    REQUIRE(gppx_rewriter_replace_literal("pink pink","pink","blue",&out)==0);
    REQUIRE(equals(out,"blue blue"));
    REQUIRE(gppx_rewriter_replace_literal("foo","","bar",&out)!=0);
    REQUIRE(gppx_rewriter_register(state,gppx_rewriter_literal_callback,&replace)==0);
    REQUIRE(gppx_process(state,"pink\n","<test>",&out)==0);
    REQUIRE(equals(out,"blue\n"));
    REQUIRE(gppx_pmatch_glob("a*\\?c?","abbb?cd")>0);
    REQUIRE(gppx_pmatch_glob("a?c","ac")==0);
    REQUIRE(gppx_pmatch_prefix("abc","abcdef")>0);
    REQUIRE(gppx_pmatch_prefix("abc","ab")==0);
    REQUIRE(gppx_pmatch_register(state,"glob",gppx_pmatch_glob_callback,NULL)==0);
    REQUIRE(gppx_process(state,"#ifmatch glob \"a*\" => \"abcd\"\nOK\n#endif\n","<test>",&out)==0);
    REQUIRE(equals(out,"OK\n"));
    REQUIRE(gppx_sink_write_file(GPPX_TEST_DATA_PATH,blob,sizeof(blob),0)==0);
    REQUIRE(gppx_reader_load_bytes(GPPX_TEST_DATA_PATH,&readback,&size)==0);
    REQUIRE(size==3 && memcmp(readback,blob,3)==0 && readback[3]==0);
    gppx_free(readback);
    REQUIRE(gppx_sink_file_callback(state,"ignored","OK",2,&sink)==0);
    REQUIRE(gppx_reader_load_bytes(GPPX_TEST_DATA_PATH,&readback,&size)==0);
    REQUIRE(size==2 && memcmp(readback,"OK",2)==0);
    gppx_free(readback);
    remove(GPPX_TEST_DATA_PATH);
    REQUIRE(gppx_embed_register(state,"custom_hex",gppx_embed_hex_callback,NULL)==0);
    /* The new helpers share engine state without changing the host ABI. */
    {
        GPPX_Style style={"mustache",";","/*","*/",0};
        GPPX_Layout layout={4,2};
        size_t offset=0,bad=0;
        uint32_t cp=0;
        const unsigned char utf[]={0x24,0xC2,0xA2,0xE2,0x82,0xAC,0xF0,0x90,0x8D,0x88};
        const unsigned char invalid[][4]={{0xC0,0x80,0,0},{0xED,0xA0,0x80,0},
            {0xF4,0x90,0x80,0x80},{0x80,0,0,0},{0xE2,0x28,0xA1,0}};
        size_t i;
        REQUIRE(gppx_style_apply(state,&style)==0);
        REQUIRE(gppx_metasym_define(state,"lang","answer","42")==0);
        REQUIRE(gppx_process(state,"{{lang_answer}} ; {{lang_answer}}\n","<style>",&out)==0);
        REQUIRE(equals(out,"42 ; {{lang_answer}}\n"));
        REQUIRE(gppx_metasym_undef(state,"lang","answer")==0);
        REQUIRE(gppx_process(state,"{{lang_answer}}\n","<style>",&out)==0);
        REQUIRE(equals(out,"lang_answer\n"));
        REQUIRE(gppx_metasym_name(NULL,"name",&out)==0);
        REQUIRE(equals(out,"name"));
        REQUIRE(gppx_metasym_name("bad-name","x",&out)!=0 && !out);
        REQUIRE(gppx_metasym_name("ok","9bad",&out)!=0 && !out);
        style.expand_strings=2;
        REQUIRE(gppx_style_apply(state,&style)!=0);
        REQUIRE(gppx_style_apply(NULL,&style)!=0);
        REQUIRE(gppx_layout_format("a\tb\r\n\r\nc\r",&layout,&out)==0);
        REQUIRE(equals(out,"  a   b\n\n  c\n"));
        REQUIRE(gppx_layout_format("",&layout,&out)==0);
        REQUIRE(equals(out,""));
        layout.tab_width=0;layout.indent_spaces=0;
        REQUIRE(gppx_layout_format("a\tb",&layout,&out)==0);
        REQUIRE(equals(out,"a\tb"));
        layout.indent_spaces=SIZE_MAX;
        REQUIRE(gppx_layout_format("x",&layout,&out)!=0 && !out);
        layout.indent_spaces=2;
        REQUIRE(gppx_layout_callback(state,"x",&out,&layout)==0);
        REQUIRE(equals(out,"  x"));
        REQUIRE(gppx_layout_format(NULL,&layout,&out)!=0 && !out);
        REQUIRE(gppx_utf8_validate(utf,sizeof utf,&bad)==0 && bad==sizeof utf);
        REQUIRE(gppx_utf8_next(utf,sizeof utf,&offset,&cp)==0 && cp==0x24 && offset==1);
        REQUIRE(gppx_utf8_next(utf,sizeof utf,&offset,&cp)==0 && cp==0xA2 && offset==3);
        REQUIRE(gppx_utf8_next(utf,sizeof utf,&offset,&cp)==0 && cp==0x20AC && offset==6);
        REQUIRE(gppx_utf8_next(utf,sizeof utf,&offset,&cp)==0 && cp==0x10348 && offset==10);
        REQUIRE(gppx_utf8_next(utf,sizeof utf,&offset,&cp)!=0 && offset==10);
        for (i=0;i<sizeof invalid/sizeof invalid[0];++i)
            REQUIRE(gppx_utf8_validate(invalid[i],4,&bad)!=0 && bad==0);
        REQUIRE(gppx_utf8_validate(utf,sizeof utf-1,&bad)!=0 && bad==6);
        REQUIRE(gppx_utf8_validate(NULL,0,&bad)==0 && bad==0);
        REQUIRE(gppx_utf8_validate(NULL,1,&bad)!=0);
        REQUIRE(gppx_sink_write_file(GPPX_TEST_DATA_PATH,"\xEF\xBB\xBF" "hello",8,0)==0);
        REQUIRE(gppx_utf8_reader_callback(state,GPPX_TEST_DATA_PATH,&out,NULL)==0);
        REQUIRE(equals(out,"hello"));
        REQUIRE(gppx_sink_write_file(GPPX_TEST_DATA_PATH,blob,sizeof blob,0)==0);
        REQUIRE(gppx_utf8_load(GPPX_TEST_DATA_PATH,&out)!=0 && !out);
        REQUIRE(gppx_sink_write_file(GPPX_TEST_DATA_PATH,invalid[0],2,0)==0);
        REQUIRE(gppx_utf8_load(GPPX_TEST_DATA_PATH,&out)!=0 && !out);
        remove(GPPX_TEST_DATA_PATH);
    }
    {
        GPPX_Profile profile={"c",NULL,NULL};
        const char *argv[]={"a,b"};
        REQUIRE(gppx_profile_count()==4);
        REQUIRE(strcmp(gppx_profile_name(3),"jackscript")==0);
        REQUIRE(gppx_profile_name(4)==NULL);
        REQUIRE(gppx_profile_apply(state,&profile)==0);
        REQUIRE(gppx_function_define(state,"WRAP","x","[x]")==0);
        REQUIRE(gppx_function_call(state,"WRAP",1,argv,&out)==0);
        REQUIRE(equals(out,"[a,b]"));
        REQUIRE(gppx_function_define(state,"BAD","x,x","x")!=0);
        REQUIRE(gppx_function_call(state,"UNKNOWN",0,NULL,&out)!=0 && !out);
        REQUIRE(gppx_function_call(state,"WRAP",-1,NULL,&out)!=0 && !out);
        REQUIRE(gppx_jackscript_enable(state)==0);
        REQUIRE(gppx_process(state,"#!/usr/bin/env jsc\n#define X 42\nconst X := $(WRAP($(X)));\n","demo.jcks",&out)==0);
        REQUIRE(equals(out,"#!/usr/bin/env jsc\nconst X := [42];\n"));
        REQUIRE(gppx_sink_write_file(GPPX_TEST_DATA_PATH,"\xEF\xBB\xBF" "#!/usr/bin/env jsc\n",22,0)==0);
        REQUIRE(gppx_jackscript_reader(state,GPPX_TEST_DATA_PATH,&out,NULL)==0);
        REQUIRE(equals(out,"#!/usr/bin/env jsc\n"));
        remove(GPPX_TEST_DATA_PATH);
    }
    gppx_destroy(state);
    printf("PASS %d C extension tests\n",count);
    return 0;
}
