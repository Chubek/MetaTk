#include "MetaPP.hpp"
#include "MetaPP-Internal.hpp"
#include <cstdlib>
#include <iostream>
static unsigned checks;
static std::string tokens(const std::string &s) {
    std::string result;
    for (const auto &t : metapp::lex(s, "test")) if (t.kind != metapp::Kind::newline) result += t.text + '\n';
    return result;
}
static void test(const std::string &source, const std::string &expected) {
    metapp::Preprocessor pp;
    ++checks;
    auto actual = pp.preprocess(source, "test.c");
    if (tokens(actual) != tokens(expected)) {
        std::cerr << "Input:\n" << source << "Expected:\n" << expected << "\nActual:\n" << actual;
        std::exit(1);
    }
}
static void reject(const std::string &source, const std::string &message) {
    metapp::Preprocessor pp; ++checks;
    try { pp.preprocess(source); }
    catch (const std::runtime_error &e) {
        if (std::string(e.what()).find(message) != std::string::npos) return;
        std::cerr << "Unexpected diagnostic: " << e.what(); std::exit(1);
    }
    std::cerr << "Unexpected success: " << source; std::exit(1);
}
int main() {
    test("#define X 7\nX + X\n#undef X\nX", "7 + 7 X");
    test("#define F(x,y) ((x)+(y))\nF(2, F(3,4))", "((2)+(((3)+(4))))");
    test("#define F(x) x+x\nF\n(1)", "1+1");
    test("#define F(x) x\n#define G F\nG(4)", "4");
    test("#define F(x) x\nF(F(1))", "1");
    test("#define A B\n#define B A\nA B", "A B");
    test("#define F(x) F(x)\nF(2)", "F(2)");
    test("#define X X+1\nX", "X+1");
    test("#define S(x) #x\n#define N 42\nS(N) S(a  +/**/ b) S(\"a\\b\")", "\"N\" \"a + b\" \"\\\"a\\\\b\\\"\"");
    test("#define C(a,b) a##b\n#define xy 9\nC(x,y) C(,x) C(x,) C(,)", "9 x x");
    test("#define C(a,b,c) a##b##c\nC(1,2,3) C(,2,)", "123 2");
    test("#define V(x,...) x(__VA_ARGS__)\nV(f,1,2) V(f) V(f,)", "f(1,2) f() f()");
    test("#define Z() 9\nZ()", "9");
    test("#define F(x) x\nF()", "");
    test("#define F (x)\nF", "(x)");
    test("a/**/b //gone\nc\\\nd", "a b cd");
    test("#define X a/*\n*/b\nX __LINE__", "a b 3");
    test("#define P(x) _Pragma(#x)\nP(pack(push,1))\nint x;\nP(pack(pop))", "#pragma pack(push,1)\nint x;\n#pragma pack(pop)");
    reject("_Pragma(42)", "string literal");
    test("#define X a\\\n+b\nX __LINE__", "a+b 3");
    test("#define A 1\n#if defined A && defined(A) && !defined(B)\nyes\n#else\nno\n#endif", "yes");
    test("#if 0\n#error skipped\n#if nonsense(\nno\n#endif\n#elif 2*3+4 == 10\nyes\n#elif 1/0\nno\n#endif", "yes");
    test("#if (0 && 1/0) || (1 ? 4 : 1/0)\nyes\n#endif", "yes");
    test("#if 1 || (1 << -1)\nyes\n#endif", "yes");
    test("#if -1 < 0 && !(-1 < 1U) && (1 ? -1 : 1U) > 0\nyes\n#endif", "yes");
    test("#if (8>>1)==4 && (~0 & 7)==7 && (5^3)==6 && 9%4==1 && 6/2==3\nyes\n#endif", "yes");
    test("#if '\\n' == 10 && '\\x41' == 'A' && '\\101' == 65 && 077 == 63 && 0xffUL == 255\nyes\n#endif", "yes");
    test("#line 100 \"virtual.c\"\n__LINE__ __FILE__\n__LINE__", "100 \"virtual.c\" 101");
    test("#define HERE __LINE__\nHERE\nHERE", "2 3");
    test("#ifdef __STDC__\n__STDC_VERSION__\n#endif", "201112L");
    test("%:define X 7\nX", "7");
    test("#define S(x) #x\nS(%:) S(%:%:) S(<:) S(:>)", "\"%:\" \"%:%:\" \"<:\" \":>\"");
    test("%:define S(x) %:x\nS(a  + b)", "\"a + b\"");
    test("%:define CAT(a,b) a %:%: b\nCAT(%,:) CAT(%:,%:)", "%: %:%:");
    test("#define S(x) #x\n#define X(x) S(x)\n#define E\nX(a E+b)", "\"a +b\"");
    test("#define S(x) #x\n#define X(x) S(x)\n#define F(x) x\nX(a F()+b)", "\"a +b\"");
    test("#define S(x) #x\n#define X(x) S(x)\n#define F(x) a x+b\nX(F())", "\"a +b\"");
    test("#define S(x) #x\n#define X(x) S(x)\n#define F(x) a x\nX(F()+b)", "\"a +b\"");
    test("#define X 1\n#define X 1\nX", "1");
    test("#pragma pack(push,1)\nint x;\n#pragma pack(pop)", "#pragma pack(push,1)\nint x;\n#pragma pack(pop)");
    reject("#if 1/0\n#endif", "division by zero");
    reject("#if 1 << 64\n#endif", "shift count");
    reject("#if 1.0\n#endif", "integer suffix");
    reject("#if 1lL\n#endif", "integer suffix");
    reject("#if 1uLl\n#endif", "integer suffix");
    reject("#if 1 +\n#endif", "missing operand");
    reject("\n#if\n#endif", "<stdin>:2: error: missing expression");
    reject("#if 0\n#elif\n#endif", "<stdin>:2: error: missing expression");
    reject("\n#include", "<stdin>:2: error: missing include");
    reject("#if defined(1)\n#endif", "identifier");
    reject("#if 1\nx", "unterminated conditional");
    reject("#else", "unmatched");
    reject("#if 1\n#else\n#else\n#endif", "after #else");
    reject("#define F(x,x) x", "duplicate parameter");
    reject("#define F(x) #y", "# must precede");
    reject("#define F(x) ##x", "either end");
    reject("#define F(x) x ## ## x", "consecutive");
    reject("#define F(x,y) x##y\nF(+,*)", "one preprocessing token");
    reject("#define F(x,y) x\nF(1)", "argument count");
    reject("#define F(x) x\nF(", "unterminated invocation");
    reject("#define A 1\n#define A 2", "redefinition");
    reject("#include \"missing-metapp-header.h\"", "not found");
    reject("#error deliberate", "deliberate");
    reject("/* bad", "unterminated comment");
    reject("\"bad", "unterminated literal");
    reject("#line 0", "out of range");
    reject("#undef __LINE__", "reserved");
    reject("#define", "<stdin>:1: error: #define requires");
    reject(std::string("before\0after", 12), "NUL byte");
    reject("#if " + std::string(300, '(') + "1" + std::string(300, ')') + "\n#endif", "nesting");
    reject(std::string(257, '\n') + "#if 1\n", "unterminated");
    {
        // Exercise budget boundaries without allocating a 64 MiB expansion.
        metapp::Engine engine;
        engine.define(metapp::lex("VALUE 1234", "limit.c"));
        engine.expansion_bytes = metapp::max_text_bytes - 4;
        ++checks;
        if (metapp::render(engine.expand(metapp::lex("VALUE", "limit.c"))) != "1234") return 1;
        ++checks;
        try { engine.expand(metapp::lex("VALUE", "limit.c")); return 1; }
        catch (const metapp::Error &e) { if (std::string(e.what()).find("byte limit") == std::string::npos) return 1; }
        engine.files_processed = metapp::max_files;
        ++checks;
        try { engine.process("", "limit.c"); return 1; }
        catch (const metapp::Error &e) { if (std::string(e.what()).find("file count") == std::string::npos) return 1; }
        // Pasted tokens are lexed in phase 4, never re-spliced or trigraph-decoded.
        ++checks;
        auto ts = metapp::lex(std::string("\"") + "??" + "=\"", "paste.c", false);
        if (ts.size() != 1 || ts[0].text != std::string("\"") + "??" + "=\"") return 1;
    }
    {
        metapp::Preprocessor pp;
        ++checks;
        try { pp.define(std::string("X 1\0ignored", 11)); return 1; }
        catch (const std::runtime_error &e) { if (std::string(e.what()).find("NUL") == std::string::npos) return 1; }
        // Exceptions must unwind include stacks and per-call resource counters.
        ++checks;
        try { pp.preprocess("#include \"unguarded.h\"", std::string(FIXTURES) + "/main.c"); return 1; }
        catch (const std::runtime_error &e) { if (std::string(e.what()).find("include nesting") == std::string::npos) return 1; }
        if (tokens(pp.preprocess("int recovered;")) != tokens("int recovered;")) return 1;
        ++checks;
        if (pp.preprocess("__FILE__", "line\n1.c") != "\"line\\0121.c\"\n") return 1;
    }
    {
        metapp::Preprocessor pp;
        pp.include_path(FIXTURES);
        auto result = pp.preprocess("#include <guard.h>\n#include <guard.h>\n#include \"once.h\"\n#include \"once.h\"\nVALUE", std::string(FIXTURES) + "/main.c");
        ++checks;
        if (tokens(result) != tokens("guard_token once_token 123")) return 1;
        result = pp.preprocess("#define HEADER <guard.h>\n#include HEADER\n#include <sub//../once.h>\n", std::string(FIXTURES) + "/main.c");
        ++checks;
        if (tokens(result) != tokens("once_token")) return 1;
        result = pp.preprocess("#line 200 \"elsewhere.c\"\n#include \"once.h\"\n__FILE__ __LINE__", std::string(FIXTURES) + "/main.c");
        ++checks;
        if (tokens(result) != tokens("once_token \"elsewhere.c\" 201")) return 1;
        pp.preprocess("#warning hello\n"); ++checks;
        if (pp.diagnostics().find("warning: hello") == std::string::npos) return 1;
    }
    std::cout << checks << " checks passed\n";
}
