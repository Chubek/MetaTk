#include "qscript/QScript.h"
#include "qscript/include/QScript-Backend.h"
#include "qscript/include/QScript-Diagnostics.h"
#include "qscript/include/QScript-Grammar.h"
#include "qscript/include/QScript-Library.h"
#include "qscript/include/QScript-Module.h"
#include "qscript/include/QScript-Runtime.h"
#include <stdio.h>
#include <string.h>

int qscript_c_smoke(void) {
    const char *source="return 6 * 7;";
    int64_t value=0;
    qs_context *c=qs_create();
    int ok=c&&qs_eval(c,source,strlen(source),&value)==QS_OK&&value==42;
    qs_destroy(c);
    return ok;
}

#define REQUIRE(x) do { if (!(x)) { fprintf(stderr,"qscript_c.c:%d: %s\n",__LINE__,#x); goto fail; } } while (0)
static int count_nodes(const qs_match *m,size_t depth,void *user) { (void)m; (void)depth; ++*(int *)user; return 1; }
static const char *resolver(const char *name,size_t *length,void *user) {
    static const char text[]="fn answer() { return 42; }";
    (void)user;
    if (strcmp(name,"host")) return NULL;
    *length=strlen(text); return text;
}
static void collect(int64_t value,void *user) { *(int64_t *)user+=value; }

int qscript_c_abi(void) {
    qs_context *c=qs_create();
    qs_program *p=NULL; qs_module *m=NULL; qs_grammar *g=NULL; qs_match *match=NULL;
    int64_t r=0,args[2]={3,4},printed=0;
    const char *text; size_t length,i;
    qs_limits limits; qs_run_info info; qs_diagnostic d; qs_backend_options bo; int nodes=0;
    const char *program="use math; fn hyp(a, b) { return isqrt(a * a + b * b); } print hyp(6, 8); return hyp(5, 12);";
    REQUIRE(c);
    /* Library ABI. */
    REQUIRE(qs_stdlib_count()>=24);
    REQUIRE(qs_stdlib_name(0)&&!qs_stdlib_name(100000));
    REQUIRE(qs_stdlib_source("bits",&length)&&length>0&&!qs_stdlib_source("nope",&length));
    REQUIRE(qs_add_library(c,"mine","const SEVEN = 7;",strlen("const SEVEN = 7;"))==QS_OK);
    REQUIRE(qs_set_library_resolver(c,resolver,NULL)==QS_OK);
    REQUIRE(qs_eval(c,"use mine, host; return SEVEN * answer();",strlen("use mine, host; return SEVEN * answer();"),&r)==QS_OK&&r==294);
    REQUIRE(qs_set_stdlib(c,0)==QS_OK);
    REQUIRE(qs_eval(c,"use math; return 0;",strlen("use math; return 0;"),&r)==QS_ERROR);
    REQUIRE(qs_clear_libraries(c)==QS_OK);
    REQUIRE(qs_eval(c,"use mine; return 0;",strlen("use mine; return 0;"),&r)==QS_ERROR);
    REQUIRE(qs_add_library(NULL,"x","",strlen(""))==QS_INVALID_ARGUMENT);
    /* Runtime ABI. */
    qs_set_output(c,collect,&printed);
    p=qs_compile(c,program,strlen(program));
    REQUIRE(p);
    REQUIRE(qs_run_ex(c,p,&info)==QS_OK&&info.result==13&&info.status==QS_RUN_HALTED&&info.outputs==1&&info.steps>0);
    REQUIRE(printed==10);
    REQUIRE(qs_call(c,p,"hyp",args,2,&r)==QS_OK&&r==5);
    REQUIRE(qs_call(c,p,"hyp",args,1,&r)==QS_ERROR);
    REQUIRE(qs_call(c,p,"nope",args,0,&r)==QS_ERROR);
    REQUIRE(qs_program_function_count(p)==2);
    for (i=0;i<qs_program_function_count(p);++i)
        if (!strcmp(qs_program_function_name(p,i),"hyp")) REQUIRE(qs_program_function_arity(p,i)==2);
    REQUIRE(qs_program_library_count(p)==1&&!strcmp(qs_program_library_name(p,0),"math"));
    REQUIRE(qs_program_size(p)>0);
    REQUIRE(qs_set_engine(c,QS_ENGINE_JIT)==QS_OK&&qs_get_engine(c)==QS_ENGINE_JIT);
    REQUIRE(qs_run(c,p,&r)==QS_OK&&r==13);
    REQUIRE(qs_set_engine(c,7)==QS_INVALID_ARGUMENT);
    REQUIRE(qs_set_engine(c,QS_ENGINE_INTERPRETER)==QS_OK);
    REQUIRE(qs_get_limits(c,&limits)==QS_OK&&limits.max_steps>0&&limits.max_depth==256);
    memset(&limits,0,sizeof limits); limits.max_steps=50;
    REQUIRE(qs_set_limits(c,&limits)==QS_OK);
    REQUIRE(qs_run_ex(c,p,&info)==QS_ERROR&&info.status==QS_RUN_STEP_LIMIT);
    limits.max_steps=10000000; limits.max_depth=1000;
    REQUIRE(qs_set_limits(c,&limits)==QS_INVALID_ARGUMENT);
    limits.max_depth=0;
    REQUIRE(qs_set_limits(c,&limits)==QS_OK);
    /* Backend ABI. */
    REQUIRE(qs_set_scheduler_name(c,"cycle")==QS_OK&&qs_get_backend(c,&bo)==QS_OK&&bo.scheduler==QS_SCHED_CYCLE);
    REQUIRE(qs_set_scheduler_name(c,"bogus")==QS_INVALID_ARGUMENT);
    REQUIRE(!strcmp(qs_scheduler_name(QS_SCHED_STATIC),"static")&&!qs_scheduler_name(9));
    REQUIRE(qs_backend_report(c,p,&text,&length)==QS_OK&&strstr(text,"blocks")&&length==strlen(text));
    REQUIRE(qs_set_scheduler_name(c,"list")==QS_OK);
    qs_program_destroy(p); p=NULL;
    /* Diagnostics ABI. */
    REQUIRE(qs_check(c,"let x = 1;\nreturn y;",strlen("let x = 1;\nreturn y;"))==QS_ERROR);
    REQUIRE(qs_diagnostic_count(c)==1&&qs_diagnostic_get(c,0,&d)==QS_OK);
    REQUIRE(d.line==2&&d.stage==QS_STAGE_VERIFY&&strstr(d.message,"undefined variable"));
    REQUIRE(!strcmp(qs_stage_name(QS_STAGE_PARSE),"parse"));
    REQUIRE(qs_diagnostic_get(c,5,&d)==QS_INVALID_ARGUMENT);
    REQUIRE(qs_check(c,"return 1;",strlen("return 1;"))==QS_OK&&qs_diagnostic_count(c)==0);
    REQUIRE(qs_ast_dump(c,"return 6*7;",strlen("return 6*7;"),&text,&length)==QS_OK&&strstr(text,"(program (return (*"));
    REQUIRE(qs_match_tree_dump(c,"return 6*7;",strlen("return 6*7;"),&text,&length)==QS_OK&&strstr(text,"statement:return"));
    REQUIRE(qs_check(c,"return (1;",strlen("return (1;"))==QS_ERROR&&qs_diagnostic_get(c,0,&d)==QS_OK&&d.stage==QS_STAGE_PARSE&&d.column==10);
    /* Module ABI. */
    m=qs_module_create();
    REQUIRE(m);
    REQUIRE(qs_module_add_library(m,"util","fn inc(x) { return x + 1; }",strlen("fn inc(x) { return x + 1; }"))==QS_OK);
    REQUIRE(qs_module_library_count(m)==1);
    REQUIRE(qs_module_add_source(m,"use util; const BASE = 40;",strlen("use util; const BASE = 40;"),"a.qs")==QS_OK);
    REQUIRE(qs_module_add_source(m,"return inc(BASE) + 1;",strlen("return inc(BASE) + 1;"),"b.qs")==QS_OK);
    REQUIRE(qs_module_compile(c,m,&p)==QS_OK&&qs_run(c,p,&r)==QS_OK&&r==42);
    qs_program_destroy(p); p=NULL;
    REQUIRE(qs_module_add_file(m,"/nonexistent/file.qs")==QS_ERROR);
    qs_module_clear(m);
    REQUIRE(qs_module_sources(m)==0&&qs_module_library_count(m)==0);
    qs_module_destroy(m); m=NULL;
    /* Grammar ABI. */
    g=qs_grammar_compile(c,"grammar G { rule TOP { <pair>+ % ',' } token pair { <ident> '=' <num> } token num { \\d+ } }",strlen("grammar G { rule TOP { <pair>+ % ',' } token pair { <ident> '=' <num> } token num { \\d+ } }"));
    REQUIRE(g);
    REQUIRE(!strcmp(qs_grammar_name(g),"G")&&qs_grammar_rule_count(g)==3&&!strcmp(qs_grammar_rule_name(g,1),"pair"));
    REQUIRE(qs_grammar_peg(c,g,&text,&length)==QS_OK&&strstr(text,"rule TOP"));
    match=qs_grammar_parse(c,g,"a=1, b=22",strlen("a=1, b=22"),NULL);
    REQUIRE(match&&!strcmp(qs_match_name(match),"TOP")&&qs_match_count(match)==2);
    {
        const qs_match *second=qs_match_child(match,1);
        const qs_match *num=qs_match_find(second,"num");
        qs_span span;
        REQUIRE(num&&!strcmp(qs_match_text(num,&length),"22")&&length==2);
        REQUIRE(qs_match_span(num,&span)==QS_OK&&span.from==7&&span.to==9&&span.column==8);
        REQUIRE(!qs_match_child(match,5)&&!qs_match_find(match,"zzz"));
    }
    REQUIRE(qs_match_walk(match,count_nodes,&nodes)==QS_OK&&nodes==7);
    REQUIRE(qs_match_dump(c,match,&text,&length)==QS_OK&&!strncmp(text,"(TOP (pair",strlen("(TOP (pair")));
    qs_match_destroy(match); match=NULL;
    REQUIRE(!qs_grammar_parse(c,g,"a=",strlen("a="),NULL)&&*qs_error(c));
    qs_grammar_destroy(g); g=NULL;
    REQUIRE(!qs_grammar_compile(c,"grammar G { rule TOP { <nope> } }",strlen("grammar G { rule TOP { <nope> } }"))&&strstr(qs_error(c),"nope"));
    g=qs_grammar_qscript(c);
    REQUIRE(g&&!strcmp(qs_grammar_name(g),"QScript")&&strstr(qs_grammar_qscript_source(),"grammar QScript"));
    match=qs_grammar_parse(c,g,"return 1;",strlen("return 1;"),"TOP");
    REQUIRE(match);
    qs_match_destroy(match); match=NULL;
    qs_grammar_destroy(g); g=NULL;
    qs_destroy(c);
    return 1;
fail:
    if (c) fprintf(stderr,"last error: %s\n",qs_error(c));
    qs_match_destroy(match); qs_grammar_destroy(g); qs_module_destroy(m); qs_program_destroy(p); qs_destroy(c);
    return 0;
}
