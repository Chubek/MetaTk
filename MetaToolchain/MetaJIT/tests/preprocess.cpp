#include "Preprocess.hpp"
#include <iostream>
#include <locale>
#include <stdexcept>

namespace {
void require(bool ok, const std::string &message) {
    if (!ok) throw std::runtime_error(message);
}
void rejects(const metajit::Preprocess &p, const std::string &source, const std::string &message) {
    try { (void)p.process(source, "negative.mjit"); }
    catch (const std::exception &e) {
        require(std::string(e.what()).find(message) != std::string::npos,
                "wrong diagnostic: " + std::string(e.what()) + " (wanted " + message + ")");
        return;
    }
    throw std::runtime_error("accepted invalid source: " + source);
}
std::string block(const std::string &body) { return "@jit_begin code\n" + body + "\n@jit_end\n"; }
struct GroupedNumbers : std::numpunct<char> {
    char do_thousands_sep() const override { return ','; }
    std::string do_grouping() const override { return "\1"; }
};
}
int main() {
    try {
        metajit::Preprocess p;
        const std::string host = "#include <vector>\n#define VALUE 3\n// @jit_begin ignored\n/*\n@jit_begin ignored\n*/\n";
        require(p.process(host) == host, "host preprocessing directives/comments changed");
        const auto source = block("fn add(x,y) { return x + y; }");
        const auto cpp = p.process(source);
        require(cpp.find("metajit_detail_buffer.append") != cpp.npos, "no inline emission");
        require(cpp.find("metajit_detail_buffer.entry(\"add\"") != cpp.npos, "missing entry");
        require(cpp == p.process(source), "output is nondeterministic");
        const auto original_locale = std::locale();
        std::locale::global(std::locale(original_locale, new GroupedNumbers));
        const auto localized_cpp = p.process(source);
        std::locale::global(original_locale);
        require(localized_cpp == cpp, "numeric locale changed generated machine code");
        require(p.process("@if 0\n" + block("nonsense") + "@endif\n").empty(), "disabled block processed");
        require(p.process("@define VALUE 42\n" + block("fn f() { return $(VALUE); }")).find("append") != cpp.npos,
                "GPPX macro not expanded");
        rejects(p, "@jit_end\n", "without jit_begin");
        rejects(p, "@jit_begin x\n", "missing jit_end");
        rejects(p, "@jit_begin x\n@jit_begin y\n", "nested");
        rejects(p, "@jit_begin x;evil\n", "identifier");
        rejects(p, "@jit_bind X value\n", "outside");
        rejects(p, block("fn f() { return 1; }\n@jit_bind X value"), "unused binding");
        rejects(p, block("@jit_bind X value\n@jit_bind X other\nfn f() { return X; }"), "duplicate binding");
        rejects(p, block("@jit_bind X value\nfn f(X) { return X; }"), "shadows");
        rejects(p, block("@jit_bind X\nfn f() { return 1; }"), "C++-EXPRESSION");
        rejects(p, block("fn f() { return external(); }"), "calls are not supported");
        rejects(p, block("fn f() { assume 1; return 2; }"), "assume");
        rejects(p, block("fn f(a,b,c,d,e,f,g) { return a; }"), "six");
        rejects(p, block("fn f() { let x = 1; }"), "without return");
        rejects(p, block("fn f() { return x; }"), "undefined variable");
        rejects(p, block("fn f() { let x = x + 1; return x; }"), "uninitialized variable x");
        rejects(p, block("fn f(c) { br c, yes, no; yes: let x=1; goto end; no: goto end; end: return x; }"), "uninitialized variable x");
        rejects(p, block("fn f(c) { goto loop; loop: br c, body, end; body: let x=1; goto loop; end: return x; }"), "uninitialized variable x");
        rejects(p, block("fn f() { goto missing; }"), "missing");
        rejects(p, block("fn f() { return ; }"), "expression");
        rejects(p, block("fn f(){return 1;} fn f(){return 2;}"), "duplicate fn");
        rejects(p, std::string("abc\0def", 7), "NUL");
        rejects(p, "@typo\n", "unknown directive");
        require(p.process(source) == cpp, "preprocessor unusable after error");
        const auto loop = block("fn f(n) { let x=0; loop: br n, body, end; body: let x=x+1; let n=n-1; goto loop; end: return x; }");
        require(!p.process(loop).empty(), "valid loop rejected");
        require(!p.process(block("fn f(c) { br c, a, b; a: let x=1; goto end; b: let x=2; goto end; end: return x; }")).empty(),
                "valid join rejected");
        std::cout << "preprocessor validation passed\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
