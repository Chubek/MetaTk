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
    gppx_destroy(state);
    printf("PASS %d C extension tests\n",count);
    return 0;
}
