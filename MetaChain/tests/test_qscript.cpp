#include "qscript/QScript.h"
#include "qscript/include/QScript-Backend.h"
#include "qscript/include/QScript-Module.h"
#include "qscript/include/QScript-Grammar.h"
#include "qscript/include/QScript-Library.h"
#include "qscript/include/QScript-Runtime.h"
#include "qscript/include/QScript-Diagnostics.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (false)
extern "C" int qscript_c_smoke(void);
extern "C" int qscript_c_abi(void);
static void language_tests();
static void grammar_tests();
int main() {
    CHECK(qscript_c_smoke());
    CHECK(qscript_c_abi());
    language_tests();
    grammar_tests();
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
    CHECK(run("return 1 && 2;").value==1);
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
    qs_program_destroy(p);
    // Backend configuration: all schedulers agree on straight-line results.
    for (auto sched : {QS_SCHED_LIST, QS_SCHED_CYCLE, QS_SCHED_STATIC}) {
        qs_backend_options bo{QS_SCHED_LIST, 1, -1};
        bo.scheduler = sched;
        CHECK(qs_set_backend(c, &bo) == QS_OK);
        CHECK(qs_set_backend(c, nullptr) == QS_INVALID_ARGUMENT);
        qs_backend_options bad{99, 0, -1};
        CHECK(qs_set_backend(c, &bad) == QS_INVALID_ARGUMENT);
        CHECK(qs_set_backend(nullptr, &bo) == QS_INVALID_ARGUMENT);
        char text[128]; size_t shown = 0;
        CHECK(qs_backend_describe(c, text, sizeof text, &shown) == QS_OK);
        CHECK(shown && text[0]);
        CHECK(qs_set_optimize(c, 1) == QS_OK && qs_set_optimize(nullptr, 1) == QS_INVALID_ARGUMENT);
        auto sp = qs_compile(c, "return 6 * 7;", 13);
        CHECK(sp);
        CHECK(qs_run(c, sp, &result) == QS_OK && result == 42);
        qs_program_destroy(sp);
    }
    {
        // Same coverage through the C++ Options surface.
        for (auto kind : {qscript::Scheduler::List, qscript::Scheduler::Cycle, qscript::Scheduler::Static}) {
            qscript::Options o;
            o.backend.scheduler = kind;
            o.backend.issue_width = 2;
            auto cp = qscript::compile("let a=1; let b=2; return a+b*3;", o);
            CHECK(cp);
            CHECK(metail::run(*cp).value == 7);
        }
        CHECK(qscript::backend::describe(qscript::Options{}).find("scheduler=") != std::string::npos);
    }
    // Modules: prelude sources share one scope with later sources.
    {
        qscript::module::Module m;
        m.sources.push_back({"let base = 40;", "prelude.qs"});
        m.sources.push_back({"return base + 2;", "user.qs"});
        auto mp = qscript::module::compile_module(m);
        CHECK(mp);
        CHECK(metail::run(*mp).value == 42);
        CHECK(!qscript::module::compile_module(qscript::module::Module{}));
        qscript::module::Module dup;
        dup.sources.push_back({"let x = 1;", "a.qs"});
        dup.sources.push_back({"let x = 2;", "b.qs"});
        CHECK(!qscript::module::compile_module(dup));
        // C API mirrors the C++ module surface.
        qs_module* mod = qs_module_create();
        CHECK(mod && qs_module_sources(mod) == 0);
        CHECK(qs_module_add_source(mod, "let base = 40;", strlen("let base = 40;"), "prelude.qs") == QS_OK);
        CHECK(qs_module_add_source(mod, "return base + 2;", strlen("return base + 2;"), "user.qs") == QS_OK);
        CHECK(qs_module_sources(mod) == 2);
        CHECK(qs_module_add_source(nullptr, "", 0, "") == QS_INVALID_ARGUMENT);
        qs_program* mp2 = nullptr;
        CHECK(qs_module_compile(c, mod, &mp2) == QS_OK && mp2);
        CHECK(qs_run(c, mp2, &result) == QS_OK && result == 42);
        qs_program_destroy(mp2);
        CHECK(qs_module_compile(c, nullptr, &mp2) == QS_INVALID_ARGUMENT);
        CHECK(qs_module_compile(nullptr, mod, &mp2) == QS_INVALID_ARGUMENT);
        qs_module_destroy(mod);
    }
    qs_destroy(c);
    std::cout << "QScript tests passed\n";
}

static std::int64_t value_of(std::string_view source, qscript::Options o = {}) {
    auto p = qscript::compile(source, o);
    if (!p) { for (const auto& d : p.diagnostics) std::cerr << metachain::format(d) << '\n'; }
    CHECK(p);
    auto r = qscript::run(*p, o);
    if (!r) { for (const auto& d : r.diagnostics) std::cerr << metachain::format(d) << '\n'; }
    CHECK(r);
    return r->value;
}
static std::string first_error(std::string_view source) {
    auto p = qscript::compile(source);
    if (p) return {};
    return metachain::format(p.diagnostics.front());
}
static void language_tests() {
    // Functions, recursion, constants, loops and operators.
    CHECK(value_of("fn fib(n) { if (n < 2) return n; return fib(n-1) + fib(n-2); } return fib(20);") == 6765);
    CHECK(value_of("sub twice(x) { return 2 * x; } return twice(twice(5));") == 20);
    CHECK(value_of("fn f() { } return f() + 1;") == 1);
    CHECK(value_of("const N = 2 ** 10 - 1; return N;") == 1023);
    CHECK(value_of("const A = 3; const B = A * A + 1; return B;") == 10);
    CHECK(value_of("let s = 0; for (let i = 0; i < 10; i++) { next if i % 2; s += i; } return s;") == 20);
    CHECK(value_of("let i = 0; loop { i++; last if i == 7; } return i;") == 7);
    CHECK(value_of("let i = 10; do { i--; } until (i <= 3); return i;") == 3);
    CHECK(value_of("let i = 0; do { i++; } while (i < 5); return i;") == 5);
    CHECK(value_of("let i = 0; until (i == 4) i++; return i;") == 4);
    CHECK(value_of("let x = 5; unless (x > 9) { x = 1; } else { x = 2; } return x;") == 1);
    CHECK(value_of("let x = 2; if (x == 1) x = 10; elsif (x == 2) x = 20; else x = 30; return x;") == 20);
    CHECK(value_of("my x = 1; x += 4; x *= 3; x -= 1; x <<= 2; x >>= 1; x %= 7; return x;") == 0);
    CHECK(value_of("return 1 < 2 ? 10 : 20;") == 10);
    CHECK(value_of("return 0 ? 1 : 0 ? 2 : 3;") == 3);
    CHECK(value_of("return (1 && 0) + (0 || 5) + (2 and 3) + (0 or 0) + not 0;") == 3);
    CHECK(value_of("return -2 ** 2;") == -4);
    CHECK(value_of("return 2 ** 3 ** 2;") == 512);
    CHECK(value_of("return (-2) ** 3 + 2 ** -1 + 1 ** -5;") == -7);
    CHECK(value_of("return 'a' + '\\n' + '\\x41';") == 97 + 10 + 65);
    CHECK(value_of("return 1_000 + 0b1010 + 0o17 + 0xff;") == 1000 + 10 + 15 + 255);
    CHECK(value_of("return true + true + false;") == 2);
    CHECK(value_of("# perl comment\n/* block */ return 1; // line") == 1);
    CHECK(value_of("let x = 0; x++; x++; x--; return x;") == 1);
    CHECK(value_of("let t = 0; return t if 1; return 9;") == 0);
    CHECK(value_of("fn g(a, b, c) { return a * 100 + b * 10 + c; } return g(1, 2, 3);") == 123);
    // Calls preserve caller variables and in-flight temporaries.
    CHECK(value_of("fn id(x) { let a = 99; return x; } let a = 1; let b = 2; return a + b * id(3) + id(4) * (b + id(5));") == 1 + 6 + 28);
    // Libraries.
    CHECK(value_of("use math; return gcd(84, 36) + math::lcm(4, 6);") == 24);
    CHECK(value_of("use math, bits; return popcount(isqrt(255));") == 4);
    CHECK(value_of("use prelude; return MASK8;") == 255);
    CHECK(value_of("use math, checked, prelude; return INT_MAX - prelude::INT_MAX;") == 0);
    {
        qscript::Options o;
        o.libraries["mylib"] = "use math; const K = 7; fn triple(x) { return 3 * abs(x); } fn _hidden() { return 1; }";
        CHECK(value_of("use mylib; return triple(-K) + mylib::K;", o) == 28);
        CHECK(!qscript::compile("use mylib; return _hidden();", o));
        CHECK(!qscript::compile("use mylib; return mylib::_hidden();", o));
        o.resolver = [](std::string_view name) -> std::optional<std::string> {
            if (name == "dyn") return std::string("fn five() { return 5; }");
            return std::nullopt;
        };
        CHECK(value_of("use dyn; return five();", o) == 5);
        o.stdlib = false;
        CHECK(!qscript::compile("use bits; return 0;", o));
    }
    // Ambiguity between libraries requires qualification.
    {
        qscript::Options o;
        o.libraries["a"] = "fn f() { return 1; }";
        o.libraries["b"] = "fn f() { return 2; }";
        CHECK(!qscript::compile("use a, b; return f();", o));
        CHECK(value_of("use a, b; return a::f() * 10 + b::f();", o) == 12);
        CHECK(value_of("use a, b; fn f() { return 3; } return f();", o) == 3);
    }
    // Diagnostics.
    CHECK(first_error("return 1") == "1:9: expected ';' at end of input");
    CHECK(first_error("if (1) { print 1;") == "1:18: expected '}' at end of input");
    CHECK(first_error("fn f(a) { return a; } return f(1, 2);").find("expects 1 argument, got 2") != std::string::npos);
    CHECK(first_error("return nope(1);").find("undefined function: nope") != std::string::npos);
    CHECK(first_error("break;").find("break outside a loop") != std::string::npos);
    CHECK(first_error("const A = 1; A = 2;").find("cannot assign to constant") != std::string::npos);
    CHECK(first_error("fn f() { return y; }").find("undefined variable: y") != std::string::npos);
    CHECK(first_error("const C = C;").find("depends on itself") != std::string::npos);
    CHECK(first_error("use nowhere;").find("unknown library: nowhere") != std::string::npos);
    CHECK(first_error("fn f() {} fn f() {}").find("duplicate function") != std::string::npos);
    CHECK(first_error("fn f(a, a) {}").find("duplicate parameter") != std::string::npos);
    CHECK(first_error("const D = 1 / 0;").find("invalid division") != std::string::npos);
    {
        qscript::Options o; o.libraries["bad"] = "print 1;";
        CHECK(!qscript::compile("use bad;", o));
    }
    {
        auto p = qscript::compile("let x = 1;\nassert x == 2;");
        CHECK(p);
        auto r = qscript::run(*p);
        CHECK(!r && r.diagnostics.front().message == "assertion failed" && r.diagnostics.front().line == 2);
    }
    // Sixteen variables per function, independently in each function.
    {
        std::string body; for (unsigned n = 0; n < 16; ++n) body += "let v" + std::to_string(n) + " = " + std::to_string(n) + ";";
        CHECK(value_of("fn f() { " + body + " return v15; } " + body + " return f() + v0;") == 15);
    }
    // Host calls into a compiled unit.
    {
        auto unit = qscript::compile_unit("use math; fn hyp(a, b) { return isqrt(a * a + b * b); } print 1; return 0;");
        CHECK(unit);
        auto r = qscript::call(*unit, "hyp", {3, 4});
        CHECK(r && r->value == 5 && r->output.empty());
        CHECK(qscript::call(*unit, "math::isqrt", {81}) && qscript::call(*unit, "math::isqrt", {81})->value == 9);
        CHECK(!qscript::call(*unit, "hyp", {1}));
        CHECK(!qscript::call(*unit, "missing", {}));
        CHECK(unit->libraries.size() == 1 && unit->libraries[0] == "math");
    }
    // Every scheduler and the unoptimized pipeline agree on a call-heavy program.
    for (auto kind : {qscript::Scheduler::List, qscript::Scheduler::Cycle, qscript::Scheduler::Static}) {
        for (bool optimize : {true, false}) {
            qscript::Options o; o.backend.scheduler = kind; o.backend.issue_width = 4; o.toolchain.optimize = optimize;
            CHECK(value_of("use combin, prime; return binomial(20, 10) + nth_prime(25) + fibonacci(30);", o) == 184756 + 97 + 832040);
        }
    }
    // The JIT runs QScript functions too.
    {
        auto p = qscript::compile("fn fib(n) { if (n < 2) return n; return fib(n-1) + fib(n-2); } return fib(22);");
        CHECK(p);
        auto j = metachain::Toolchain().jit(*p);
        CHECK(j && j->value == 17711);
    }
    // Stdlib introspection.
    CHECK(qscript::stdlib_names().size() >= 24);
    CHECK(qscript::stdlib_source("math") && !qscript::stdlib_source("nope"));
    for (const auto& name : qscript::stdlib_names()) {
        auto ok = qscript::compile("use " + name + "; return 0;");
        if (!ok) std::cerr << name << ": " << metachain::format(ok.diagnostics.front()) << '\n';
        CHECK(ok);
    }
}
static void grammar_tests() {
    using qscript::grammar::Grammar;
    auto g = Grammar::compile(R"(
        # comments are allowed
        grammar Base {
            token num  { \d+ }
        }
        grammar Calc is Base {
            rule  TOP  { <expr> }
            rule  expr { <term>+ % <op> }
            token term { <num> | '(' ~ ')' <expr> }
            proto token op {*}
            token op:sym<+>  { <sym> }
            token op:sym<->  { <sym> }
            token op:sym<**> { <sym> }
            token op:sym<*>  { <sym> }
            token word { <alpha> \w* }
            token kw   { 'if' >> }
            token cmt  { '/*' .*? '*/' }
            token list { '[' [<num>* %% ','] ']' }
            token hex  { <xdigit> ** 2..4 }
            token notq { <-['"]>+ }
            token ab   { <first=num> '-' <second=num> }
            token peek { <?before 'x'> . <!before 'y'> . }
        }
    )");
    if (!g.ok()) std::cerr << g.error << '\n';
    CHECK(g.ok() && g.name == "Calc");
    auto r = g.parse("1 + 2 ** (3 - 4)");
    CHECK(r);
    CHECK(r.match->dump() == "(TOP (expr (term (num \"1\")) (op:+ \"+\") (term (num \"2\")) (op:** \"**\") (term (expr (term (num \"3\")) (op:- \"-\") (term (num \"4\"))))))");
    CHECK(r.match->find("expr")->all("op").size() == 2);
    CHECK(r.match->to_ast().tag() == "TOP");
    CHECK(g.parse("abc_9", "word"));
    CHECK(g.parse("if", "kw") && !g.parse("iffy", "kw"));
    CHECK(g.parse("/* a * b */", "cmt"));
    CHECK(g.parse("[1,2,3,]", "list") && g.parse("[]", "list") && !g.parse("[,]", "list"));
    CHECK(g.parse("abc", "hex") && !g.parse("a", "hex") && !g.parse("abcde", "hex"));
    CHECK(g.parse("plain text", "notq") && !g.parse("it's", "notq"));
    auto ab = g.parse("12-34", "ab");
    CHECK(ab && ab.match->find("first")->text == "12" && ab.match->find("second")->text == "34");
    CHECK(g.parse("xz", "peek") && !g.parse("xy", "peek") && !g.parse("zz", "peek"));
    auto bad = g.parse("1 +");
    CHECK(!bad && bad.line == 1 && bad.column == 4);
    CHECK(bad.error.find("expected") == 0);
    // Construction errors.
    CHECK(!Grammar::compile("grammar G { rule TOP { <missing> } }").ok());
    CHECK(!Grammar::compile("grammar G { rule TOP { 'a' { code } } }").ok());
    CHECK(Grammar::compile("grammar G { rule TOP { a + } }").ok());
    CHECK(!Grammar::compile("grammar G { proto token p {*} }").ok());
    CHECK(!Grammar::compile("not a grammar").ok());
    // Programmatic construction.
    Grammar h;
    h.name = "Pairs";
    h.rule("TOP", "<pair>+ % ','").token("pair", "<key=ident> '=' <value=num>").token("num", "'-'? \\d+");
    CHECK(h.build());
    auto pr = h.parse("a=1, b=-2");
    CHECK(pr && pr.match->all("pair").size() == 2 && pr.match->children[1].find("value")->text == "-2");
    // The QScript grammar itself is available.
    CHECK(qscript::language_grammar().ok());
    CHECK(std::string(qscript::grammar_source()).find("grammar QScript") != std::string::npos);
    CHECK(qscript::language_grammar().parse("fn f(x) { return x; }"));
}
