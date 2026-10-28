#include "MetaIL.h"
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr,"%s:%d: %s (%s)\n",__FILE__,__LINE__,#condition,metail_last_error(c));return 1; \
} } while (0)
int main(int argc, char **argv) {
    if (argc<2) return 1;
    char error[2048];metail_context *c=metail_create(argv[1],error,sizeof error);
    if (!c) { fprintf(stderr,"%s\n",error);return 1; }
    metail_expr x={.kind=METAIL_VAR,.name="x"},zero={.kind=METAIL_INT,.value=0};
    const metail_expr *args[]={&x,&zero};
    metail_expr add={.kind=METAIL_OP,.op="add",.args=args,.arg_count=2};
    const char *params[]={"x"};
    metail_stmt statements[]={ {.kind=METAIL_LABEL,.name="entry"}, {.kind=METAIL_RETURN,.expr=&add} };
    metail_function function={.name="f",.params=params,.param_count=1,.statements=statements,.statement_count=2};
    metail_module module={.name="native",.functions=&function,.function_count=1};
    const char *abi_params[]={"i64"};
    metail_external external={.name="magnitude",.symbol="labs",.library="c",.result="i64",.params=abi_params,.param_count=1};
    module.externals=&external;module.external_count=1;
    CHECK(metail_module_check(c,&module)==METAIL_OK);
    char *text=NULL,*stats=NULL;
    CHECK(metail_module_emit(c,&module,&text)==METAIL_OK);
    CHECK(strstr(text,"return (x + 0);") && strstr(text,"module native;"));
    if (argc==2) {
        metail_module *parsed=NULL;
        CHECK(metail_parse(c,text,"roundtrip.mi",&parsed)==METAIL_OK);
        CHECK(parsed->function_count==1 && parsed->functions[0].statement_count==2);
        CHECK(parsed->external_count==1 && !strcmp(parsed->externals[0].symbol,"labs"));
        CHECK(!strcmp(parsed->functions[0].statements[1].expr->op,"add"));
        metail_module_free(parsed);
    } else {
        CHECK(metail_check(c,"fn f() {}",NULL)==METAIL_RUNTIME_ERROR);
        CHECK(strstr(metail_last_error(c),"TEXT PARSER DISABLED"));
    }
    metail_free(text);
    metail_module *optimized=NULL;
    CHECK(metail_module_optimize(c,&module,"AddZeroRight",2,&optimized,&stats)==METAIL_OK);
    CHECK(optimized->functions[0].statements[1].expr->kind==METAIL_VAR);
    CHECK(optimized->external_count==1 && !strcmp(optimized->externals[0].params[0],"i64"));
    CHECK(metail_module_linkage(c,optimized,&text)==METAIL_OK);
    CHECK(strstr(text,"\"arguments\":[\"-lc\"]") && strstr(text,"\"symbol\":\"labs\""));metail_free(text);
    CHECK(strstr(stats,"\"AddZeroRight\":1"));metail_free(stats);
    CHECK(add.kind==METAIL_OP && add.args[1]->value==0); // input still intact
    CHECK(metail_module_analyze(c,optimized,"ControlFlowGraph,VariableUses",&text)==METAIL_OK);
    CHECK(strstr(text,"ControlFlowGraph") && strstr(text,"entry"));metail_free(text);
    metail_module *bad=NULL;
    CHECK(metail_module_optimize(c,&module,"NoSuchPass",1,&bad,&stats)==METAIL_RUNTIME_ERROR && !bad && !stats);
    CHECK(metail_module_optimize(c,&module,NULL,0,&bad,&stats)==METAIL_INVALID_ARGUMENT && !bad && !stats);
    CHECK(metail_module_analyze(c,&module,"AddZeroRight",&text)==METAIL_RUNTIME_ERROR && !text);
    add.arg_count=1;CHECK(metail_module_check(c,&module)==METAIL_RUNTIME_ERROR);add.arg_count=2;
    add.op="missing";CHECK(metail_module_check(c,&module)==METAIL_RUNTIME_ERROR);add.op="add";
    x.name="x; return 1";CHECK(metail_module_check(c,&module)==METAIL_RUNTIME_ERROR);x.name="x";
    statements[0].name="return";CHECK(metail_module_check(c,&module)==METAIL_RUNTIME_ERROR);statements[0].name="entry";
    args[1]=&add;CHECK(metail_module_check(c,&module)==METAIL_RUNTIME_ERROR);args[1]=&zero;
    add.args=NULL;CHECK(metail_module_check(c,&module)==METAIL_RUNTIME_ERROR);add.args=args;
    function.param_count=200001;CHECK(metail_module_check(c,&module)==METAIL_RUNTIME_ERROR);function.param_count=1;
    statements[0].kind=METAIL_GOTO;statements[0].target="missing";
    CHECK(metail_module_check(c,&module)==METAIL_RUNTIME_ERROR);
    statements[0].kind=METAIL_LABEL;
    zero.value=INT64_MIN;
    CHECK(metail_module_optimize(c,&module,"AddZeroRight",1,&bad,NULL)==METAIL_OK);
    CHECK(bad->functions[0].statements[1].expr->args[1]->value==INT64_MIN);metail_module_free(bad);
    CHECK(metail_module_check(c,NULL)==METAIL_INVALID_ARGUMENT);
    external.variadic=2;CHECK(metail_module_check(c,&module)==METAIL_RUNTIME_ERROR);external.variadic=0;
    external.params=NULL;CHECK(metail_module_check(c,&module)==METAIL_RUNTIME_ERROR);external.params=abi_params;
    external.result="f64";CHECK(metail_module_check(c,&module)==METAIL_RUNTIME_ERROR);external.result="i64";
    module.externals=NULL;CHECK(metail_module_check(c,&module)==METAIL_RUNTIME_ERROR);module.externals=&external;
    CHECK(metail_operator_arity("select")==3 && metail_operator_arity("neg")==1 && metail_operator_arity("invalid")==-1);
    metail_destroy(c);c=NULL;
    // Exported trees have independent ownership and can enter a different runtime.
    CHECK(!strcmp(optimized->functions[0].statements[1].expr->name,"x"));
    CHECK(!strcmp(optimized->externals[0].library,"c"));
    c=metail_create(argv[1],error,sizeof error);CHECK(c);
    CHECK(metail_module_emit(c,optimized,&text)==METAIL_OK);metail_free(text);
    metail_module_free(optimized);metail_module_free(NULL);metail_destroy(c);
    return 0;
}
