#include "MetaIL.h"
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#condition);return 1; \
} } while (0)

int main(int argc, char **argv) {
    CHECK(argc==2);
    char error[2048];
    CHECK(metail_create("/nonexistent/metail",error,sizeof error)==NULL);
    CHECK(strstr(error,"cannot locate"));
    metail_context *c=metail_create(argv[1],error,sizeof error);
    CHECK(c && !error[0]);
    char *text=NULL,*stats=NULL;
    CHECK(metail_emit(c,"fn f() { return @; }","broken.mi",&text)==METAIL_RUNTIME_ERROR);
    CHECK(!text && strstr(metail_last_error(c),"broken.mi"));
    CHECK(metail_check(c,"fn f() { goto nowhere; }",NULL)==METAIL_RUNTIME_ERROR);
    CHECK(metail_check(c,"fn f(x,x) { return x; }",NULL)==METAIL_RUNTIME_ERROR);
    CHECK(metail_emit(c,"fn f() { return 1; }",NULL,&text)==METAIL_OK);
    CHECK(strstr(text,"return 1;") && !*metail_last_error(c));metail_free(text);
    CHECK(metail_optimize(c,"fn f(x) { return x + 0; }",NULL,"AddZeroRight",2,&text,&stats)==METAIL_OK);
    CHECK(strstr(text,"return x;") && strstr(stats,"\"AddZeroRight\":1"));
    metail_free(text);metail_free(stats);
    CHECK(metail_optimize(c,"fn f() { return 1; }",NULL,NULL,0,&text,&stats)==METAIL_INVALID_ARGUMENT);
    CHECK(!text && !stats);
    CHECK(metail_optimize(c,"fn f() { return 1; }",NULL,NULL,2,&text,&text)==METAIL_INVALID_ARGUMENT);
    CHECK(!text);
    CHECK(metail_optimize(c,"fn f() { return tick() * 0; }",NULL,"MultiplyZeroRight",1,&text,NULL)==METAIL_OK);
    CHECK(strstr(text,"tick()") && strstr(text,"* 0"));metail_free(text);
    CHECK(metail_optimize(c,"fn f() { return (1 / 0) * 0; }",NULL,"MultiplyZeroRight",1,&text,NULL)==METAIL_OK);
    CHECK(strstr(text,"/ 0") && strstr(text,"* 0"));metail_free(text);
    CHECK(metail_optimize(c,"fn f() { return 9223372036854775807 + 1; }",NULL,"ConstantFolding",1,&text,NULL)==METAIL_OK);
    CHECK(strstr(text,"-9223372036854775808"));metail_free(text);
    CHECK(metail_optimize(c,"fn f(x) { return (x & 511) < 512; }",NULL,
        "WeighstoneRanges",1,&text,&stats)==METAIL_OK);
    CHECK(strstr(text,"return 1;") && strstr(stats,"\"WeighstoneRanges\":1"));
    metail_free(text);metail_free(stats);
    CHECK(metail_analyze(c,"fn f(x) { return x & 511; }",NULL,"WeighstoneIntegerFacts",&text)==METAIL_OK);
    CHECK(strstr(text,"\"max\":511") && strstr(text,"\"discardable\":true"));metail_free(text);
    CHECK(metail_analyze(c,"fn f(x) { return x+1; }",NULL,"InstructionCount",&text)==METAIL_OK);
    CHECK(strstr(text,"InstructionCount"));metail_free(text);
    CHECK(metail_analyze(c,"fn f() {}",NULL,"AddZeroRight",&text)==METAIL_RUNTIME_ERROR && !text);
    CHECK(metail_analyze(c,"fn f() {}",NULL,"",&text)==METAIL_RUNTIME_ERROR && !text);
    CHECK(metail_analyze(c,"fn f() {}",NULL,"InstructionCount,",&text)==METAIL_RUNTIME_ERROR && !text);
    size_t count=0;
    CHECK(metail_kernel_count(c,&count)==METAIL_OK && count==327);
    metail_kernel_info info;
    CHECK(metail_kernel_get(c,count,&info)==METAIL_RUNTIME_ERROR && !info.name);
    CHECK(metail_kernel_get(c,0,&info)==METAIL_OK && info.name && strstr(info.path,".optk"));
    CHECK(metail_kernels_json(c,&text)==METAIL_OK && strstr(text,"\"count\":327"));metail_free(text);
    CHECK(metail_load_kernels(c,"/nonexistent/metail")==METAIL_RUNTIME_ERROR);
    CHECK(metail_kernel_count(c,&count)==METAIL_OK && count==327);
    CHECK(metail_check(c,NULL,NULL)==METAIL_INVALID_ARGUMENT);
    CHECK(metail_check(NULL,"fn f() {}",NULL)==METAIL_INVALID_ARGUMENT);
    CHECK(metail_emit(c,"fn f() {}",NULL,NULL)==METAIL_INVALID_ARGUMENT);
    metail_destroy(c);metail_destroy(NULL);metail_free(NULL);
    return 0;
}
