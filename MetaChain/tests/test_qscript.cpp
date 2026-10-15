#include "qscript/QScript.h"
#include <cstdlib>
#include <iostream>

#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (false)
extern "C" int qscript_c_smoke(void);
int main() {
    CHECK(qscript_c_smoke());
    auto run=[](std::string_view source) {
        auto p=qscript::compile(source); if (!p) { for (const auto& d:p.diagnostics) std::cerr << d.message << '\n'; }
        CHECK(p); return metail::run(*p);
    };
    CHECK(run("return 6 * 7;").value==42);
    CHECK(run("let n=5; let f=1; while(n>1) { f=f*n; n=n-1; } print f; return f;").value==120);
    CHECK(run("let x=1; if (x) { let x=4; print x; } else { print 8; } return x;").value==1);
    CHECK(run("return 2+3*4<<1;").value==28);
    CHECK(run("return -9223372036854775808;").value==INT64_MIN);
    CHECK(run("return (2<=2) + (3>=4) + (4!=5) + !7;").value==2);
    CHECK(run("return ~0;").value==-1);
    CHECK(run("// comment\n return 0xff;").value==255);
    CHECK(!run("return 1 / 0;"));
    CHECK(!qscript::compile("return absent;"));
    CHECK(!qscript::compile("let x=1; let x=2;"));
    CHECK(!qscript::compile("if(1) let x=1; return x;"));
    CHECK(!qscript::compile("return 1"));
    CHECK(!qscript::compile("return 9223372036854775808;"));
    CHECK(!qscript::compile("return 1 && 2;"));
    CHECK(!qscript::compile(std::string("return 1;\0return 2;",19)));
    auto ast=qscript::parse("let x=1; return x;"); CHECK(ast); std::size_t visited=0;
    CHECK(qscript::visit(*ast,[&](const auto&){++visited;return true;})); CHECK(visited>3);
    CHECK(!qscript::visit(*ast,[](const auto&){return true;},0));
    qscript::Options small; small.max_nodes=2; CHECK(!qscript::parse("return 1;",small));
    small={}; small.max_depth=2; CHECK(!qscript::parse("return (((1)));",small));
    small={}; small.toolchain.il.max_instructions=1; CHECK(!qscript::compile("return 1;",small));
    std::string too_deep="return 1"; for (unsigned n=0;n<2000;++n) too_deep+="+1"; too_deep+=';';
    CHECK(!qscript::compile(too_deep));
    std::string too_many; for (unsigned n=0;n<17;++n) too_many+="let v"+std::to_string(n)+"=1;";
    CHECK(!qscript::compile(too_many));
    // A long block takes the bounded scheduling fallback, retaining semantics.
    std::string long_block="let n=0;"; for (unsigned n=0;n<300;++n) long_block+="n=n+1;"; long_block+="return n;";
    CHECK(run(long_block).value==300);
    qs_context* c=qs_create(); CHECK(c); std::int64_t result=0;
    qs_set_step_limit(c,20); CHECK(qs_eval(c,"while(1) {}",11,&result)==QS_ERROR); CHECK(*qs_error(c));
    CHECK(qs_eval(c,nullptr,1,&result)==QS_ERROR);
    CHECK(qs_eval(nullptr,"",0,&result)==QS_INVALID_ARGUMENT);
    qs_set_step_limit(c,100000);
    auto p=qs_compile(c,"return 42;",10); CHECK(p);
    for (auto format:{QS_IL,QS_BYTECODE,QS_NATIVE,QS_OBJECT,QS_EXECUTABLE}) {
        const unsigned char* bytes=nullptr; std::size_t size=0;
        CHECK(qs_emit(c,p,format,&bytes,&size)==QS_OK); CHECK(bytes&&size);
        if (format==QS_BYTECODE) CHECK(metail::decode({bytes,size}));
    }
    const unsigned char* bytes=nullptr; std::size_t size=0;
    CHECK(qs_emit(c,p,99,&bytes,&size)==QS_INVALID_ARGUMENT);
    CHECK(!bytes&&!size); CHECK(qs_run(c,p,&result)==QS_OK&&result==42); CHECK(!*qs_error(c));
    qs_program_destroy(p); qs_destroy(c);
    std::cout << "QScript tests passed\n";
}
