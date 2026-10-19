#include "MetaPP.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
int failures = 0;
int checks = 0;
void check(bool value, const std::string& label) {
    ++checks;
    if (!value) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}
std::vector<std::string> spellings(std::string_view s) {
    std::vector<std::string> result;
    for (const auto& t : metapp::tokenize(s)) result.push_back(t.text);
    return result;
}
void expect(std::string_view source, std::string_view expected, const std::string& label) {
    try {
        metapp::Preprocessor pp;
        auto result = pp.process(source);
        check(spellings(result) == spellings(expected), label + " (actual: " + result + ")");
    } catch (const std::exception& e) { check(false, label + ": " + e.what()); }
}
void error(std::string_view source, std::string_view message) {
    try {
        metapp::Preprocessor pp;
        pp.process(source);
        check(false, "expected error: " + std::string(message));
    } catch (const metapp::Error& e) {
        check(std::string(e.what()).find(message) != std::string::npos, "diagnostic: " + std::string(e.what()));
    }
}
struct Temp {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("metapp-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temp() { std::filesystem::create_directory(path); }
    ~Temp() { std::error_code ec; std::filesystem::remove_all(path, ec); }
    void write(const std::string& name, std::string_view text) { std::ofstream(path / name) << text; }
};
} // namespace

int main() {
    expect("int x = 3; // comment\n/* block */x++;", "int x = 3; x++;", "lex and comments");
    expect("#define X 4\nX X_SUFFIX \"X\" 'X'", "4 X_SUFFIX \"X\" 'X'", "object macros");
    expect("#define X 4\n#define Y X+1\nY\n#undef X\nY", "4+1 X+1", "rescan and undef");
    expect("#define EMPTY\na EMPTY b", "a b", "empty macro");
    expect("#define A A\nA\n#define B C\n#define C B\nB C", "A B C", "recursive hide sets");
    expect("#define F(x,y) (x)+(y)\nF(1,(2,3))", "(1)+((2,3))", "nested arguments");
    expect("#define F(x) x+x\nF(F(2))", "2+2+2+2", "argument prescan");
    expect("#define F(x) x\n#define G F\nG (123)", "123", "invocation across replacement boundary");
    expect("#define F(x) x\nF\n(123)", "123", "multiline invocation");
    expect("#define F() yes\nF() F", "yes F", "zero argument function");
    expect("#define F(x) [x]\nF()", "[]", "empty argument");
    expect("#define S(x) #x\nS( a   + /* hi */ b ) S(\"hello\")", "\"a + b\" \"\\\"hello\\\"\"", "stringification");
    expect("#define S(x) #x\n#define X 1\nS(X)", "\"X\"", "stringify without prescan");
    expect("#define C(a,b) a##b\n#define xy 7\nC(x,y)", "7", "paste and rescan");
    expect("#define C(a,b) a##b\nC(,x) C(x,) C(,) C(a b,c d)", "x x a bc d", "paste placemarkers");
    expect("#define C(a,b,c) a##b##c\nC(a,,c) C(,,)", "ac", "chained paste");
    expect("#define V(a,...) a(__VA_ARGS__)\nV(f,1,2) V(g) V(h,)", "f(1,2) g() h()", "variadic macros");
    expect("#define V(...) __VA_ARGS__\nV() V(a,b)", "a,b", "variadic only");
    expect("#define X 1 + \\\n2\nX", "1+2", "line splicing");
    expect("#define X 4\r\nX\r\n", "4", "CRLF");
    expect("#define X 1\n#if defined(X) && !defined MISSING\nyes\n#else\nno\n#endif", "yes", "defined");
    expect("#if 0\n#error skipped\n#define X 1\n#if (broken\nno\n#endif\n#elif 2+3*4 == 14\nyes\n#else\nno\n#endif\nX", "yes X", "inactive groups");
    expect("#if 1 || 1/0\na\n#endif\n#if 0 && 1/0\nno\n#endif", "a", "short circuit");
    expect("#if (0 ? 1/0 : 3) == 3 && (1 ? 4 : 1/0) == 4\nyes\n#endif", "yes", "conditional expression");
    expect("#if -1 < 0 && !(-1 < 0U) && (0xFF & 0xf) == 15 && (1 << 3) == 8\nyes\n#endif", "yes", "integer signedness and bitwise");
    expect("#if '\\n' == 10 && '\\x41' == 'A' && '\\101' == 'A'\nyes\n#endif", "yes", "character literals");
    expect("#if 0b101 == 5 && 010 == 8 && 42ULL == 42\nyes\n#endif", "yes", "integer bases and suffixes");
    expect("__LINE__\n__LINE__ __FILE__ __COUNTER__ __COUNTER__", "1 2 \"<input>\" 0 1", "builtins");
    expect("#line 100 \"virtual.c\"\n__LINE__ __FILE__\n__LINE__", "100 \"virtual.c\" 101", "line directive");
    expect("#define L __LINE__\nL\nL", "2 3", "expansion locations");
    expect("#define A 1\n#define A 1\nA", "1", "identical redefinition");
    expect("%:define A 1\nA", "1", "directive digraph");
    expect("#define S(x) #x\nS(%:) S(%:%:)", "\"%:\" \"%:%:\"", "digraph spelling in stringification");
    expect("%:define S(x) %:x\n%:define C(a,b) a %:%: b\nS(hi) C(hel,lo)", "\"hi\" hello", "digraph macro operators");
    expect("#define S(...) #__VA_ARGS__\nS(a , b) S(a,b)", "\"a , b\" \"a,b\"", "variadic comma whitespace");
    expect("#define E\n#define S_(x) #x\n#define S(x) S_(x)\nS(a E+b)", "\"a +b\"", "empty macro whitespace");
    expect("#define F(x) #x\n#define G(x) F(a x+b)\nG()", "\"a +b\"", "empty parameter whitespace");
    expect("#define F(x) a x\n#define S_(x) #x\n#define S(x) S_(x)\nS(F()+b)", "\"a +b\"", "trailing empty parameter whitespace");
    expect("#line 3 \"a\\\\b.c\"\n__FILE__", "\"a\\\\b.c\"", "escaped line filename");
    expect("#line 3 \"a\\x2e\\143\"\n__FILE__", "\"a.c\"", "numeric line filename escapes");
    expect("#if 1'000 == 1000 && 0xA'B == 171 && 0'7 == 7\nyes\n#endif", "yes", "digit separators");
    expect("#rewrite old ( ) => new ( 1 )\nold() older()", "new(1) older()", "source rewrite");
    expect("#rewrite a => b\n#rewrite b => a\na b", "b a", "nonrecursive rewriting");
    expect("#kernel eval (2 + 3) * 4\n#kernel eval -9 / 2\n#kernel eval ~0U", "20 -4 18446744073709551615U", "integer expression kernel");
    expect("#define X 9\n#kernel stringify hello X\n#kernel concat ident ifier", "\"hello 9\" identifier", "text kernels");

    error("#if 1\n", "unterminated conditional");
    error("#else", "unmatched");
    error("#if 1\n#else\n#else\n#endif", "after #else");
    error("#if 1\n#endif x", "unexpected tokens");
    error("#if 1/0\n#endif", "division by zero");
    error("#if 1 << -1\n#endif", "shift count");
    error("#if 2.0\n#endif", "invalid integer");
    error("#if defined()\n#endif", "identifier after defined");
    error("#if (1\n#endif", "expected ')'");
    error("#if\n#endif", "empty preprocessor expression");
    error("#define F(a,a) a", "duplicate");
    error("#define F(x) # y", "must precede");
    error("#define F(x) x ##", "edge");
    error("#define F(a,b) a\nF(1)", "wrong number");
    error("#define F(x) x\nF(", "unterminated invocation");
    error("#define F(a,b) a##b\nF(+,*)", "token paste");
    error("#define X 1\n#define X 2", "incompatible redefinition");
    error("#define __LINE__ 4", "reserved macro");
    error("#define X __VA_ARGS__", "requires a variadic");
    error("/* unfinished", "unterminated comment");
    error("\"unfinished", "unterminated literal");
    error("#error stop here", "#error stop here");
    error("#unknown", "unknown directive");
    error("#kernel missing data", "unknown or missing kernel");
    error("#rewrite => bad", "expected #rewrite");
    error("#include \"missing-metapp-file.h\"", "not found");
    error("#line 0", "invalid #line number");
    error("#if 1'U\n#endif", "invalid digit separator");
    error("#if 1lL\n#endif", "invalid integer suffix");
    error("#line 1 \"a\\0\"", "NUL in #line filename");
    error("#include <unfinished", "unterminated include path");
    error("#include \"some.h\" extra", "unexpected tokens after include path");
    error(std::string("int a;\0int b;", 14), "NUL byte in source");

    metapp::Preprocessor pp;
    pp.define("CLI=9");
    pp.define("FN(x)=x+1");
    check(spellings(pp.process("CLI FN(2)")) == spellings("9 2+1"), "API definitions");
    pp.undefine("CLI");
    check(!pp.is_defined("CLI") && pp.is_defined("__LINE__"), "API undef and builtins");
    pp.process("#warning careful\n#pragma unrecognized\n");
    check(pp.warnings().size() == 2, "warning collection");
    pp.process("");
    check(pp.warnings().empty(), "diagnostics reset");
    pp.register_kernel("reverse", [](const metapp::Tokens& args, const metapp::Location&) {
        return metapp::Tokens(args.rbegin(), args.rend());
    });
    pp.register_directive("emit", [](metapp::Preprocessor&, const metapp::Tokens& args, const metapp::Location&) { return args; });
    pp.add_rewrite(metapp::tokenize("old"), metapp::tokenize("new"));
    check(spellings(pp.process("#define A 7\n#kernel reverse x A\n#emit old\n#if 0\n#kernel missing\n#endif")) ==
          spellings("7 x new"), "extension pipeline and inactive skip");
    pp.clear_rewrites();
    check(spellings(pp.process("old")) == spellings("old"), "clear rewrites");
    try { pp.add_rewrite({}, {}); check(false, "empty rewrite rejected"); }
    catch (const std::invalid_argument&) { check(true, "empty rewrite rejected"); }
    try { pp.register_directive("if", {}); check(false, "reserved directive rejected"); }
    catch (const std::invalid_argument&) { check(true, "reserved directive rejected"); }
    try { pp.define("A B=1"); check(false, "invalid API signature"); }
    catch (const metapp::Error&) { check(true, "invalid API signature"); }
    pp.register_directive("reenter", [](metapp::Preprocessor& instance, const metapp::Tokens&, const metapp::Location&) {
        instance.process("nested");
        return metapp::Tokens{};
    });
    try { pp.process("#reenter"); check(false, "reentrancy guard"); }
    catch (const metapp::Error& e) { check(std::string(e.what()).find("reentrant") != std::string::npos, "reentrancy guard"); }
    check(pp.process("valid") == "valid\n", "reuse after callback exception");
    pp.register_directive("replace", [](metapp::Preprocessor& instance, const metapp::Tokens&, const metapp::Location& where) {
        instance.register_directive("replace", [](metapp::Preprocessor&, const metapp::Tokens&, const metapp::Location& next) {
            return metapp::tokenize("second", next);
        });
        return metapp::tokenize("first", where);
    });
    check(spellings(pp.process("#replace\n#replace")) == spellings("first second"), "safe directive self-replacement");
    pp.register_kernel("count", [count = 0](const metapp::Tokens&, const metapp::Location& where) mutable {
        return metapp::tokenize(std::to_string(++count), where);
    });
    check(spellings(pp.process("#kernel count\n#kernel count")) == spellings("1 2"), "mutable callback state persists");
    pp.register_kernel("replace_kernel", [&pp](const metapp::Tokens&, const metapp::Location& where) {
        pp.register_kernel("replace_kernel", [](const metapp::Tokens&, const metapp::Location& next) {
            return metapp::tokenize("second", next);
        });
        return metapp::tokenize("first", where);
    });
    check(spellings(pp.process("#kernel replace_kernel\n#kernel replace_kernel")) == spellings("first second"), "safe kernel self-replacement");
    try { pp.process("#undef __LINE__", "located.c"); check(false, "located undef error"); }
    catch (const metapp::Error& e) { check(e.location.file == "located.c", "located undef error"); }

    {
        Temp temp;
        temp.write("once.h", "#pragma once\n#define VALUE 42\nheader\n");
        temp.write("guard.h", "#ifndef GUARD_H\n#define GUARD_H\n#include \"guard.h\"\nguarded\n#endif\n");
        temp.write("main.c", "#define HEADER \"once.h\"\n#include HEADER\n#include \"once.h\"\n#include <guard.h>\nVALUE\n");
        metapp::Options options;
        options.include_paths.push_back(temp.path);
        metapp::Preprocessor files(options);
        check(spellings(files.process_file(temp.path / "main.c")) == spellings("header guarded 42"), "includes, guards and pragma once");
        check(files.dependencies().size() == 3, "unique dependency tracking");
        check(spellings(files.process_file(temp.path / "once.h")) == spellings("header"), "pragma once resets per call");
        temp.write("HEADER.h", "literal_header\n");
        temp.write("two words.h", "space_header\n");
        std::filesystem::create_directory(temp.path / "dir");
        temp.write("dir/nested.h", "nested_header\n");
        temp.write("trail\\", "backslash_header\n");
        temp.write("literal.c", "#undef HEADER\n#define HEADER changed\n#define two changed\n#define words changed\n#include <HEADER.h>\n#include <two words.h>\n#include <dir//nested.h>\n#include \"trail\\\"\n");
        check(spellings(files.process_file(temp.path / "literal.c")) ==
              spellings("literal_header space_header nested_header backslash_header"), "opaque literal include names");
        check(spellings(files.process("#define H <two words.h>\n#undef two\n#undef words\n#include H", (temp.path / "input.c").string())) ==
              spellings("space_header"), "macro include preserves header whitespace");
        temp.write("cycle.h", "#include \"cycle.h\"\n");
        options.max_include_depth = 4;
        metapp::Preprocessor cycle(options);
        try { cycle.process_file(temp.path / "cycle.h"); check(false, "include depth limit"); }
        catch (const metapp::Error& e) { check(std::string(e.what()).find("depth limit") != std::string::npos, "include depth limit"); }
    }
    {
        metapp::Options options;
        options.max_expansions = 3;
        metapp::Preprocessor limited(options);
        try { limited.process("#define X 1\nX X X X"); check(false, "expansion limit"); }
        catch (const metapp::Error&) { check(true, "expansion limit"); }
        options.max_output_tokens = 3;
        metapp::Preprocessor tokens(options);
        try { tokens.process("one two three four"); check(false, "token limit"); }
        catch (const metapp::Error&) { check(true, "token limit"); }
        try { tokens.process("a b\n#warning split\nc d"); check(false, "total token limit"); }
        catch (const metapp::Error&) { check(true, "total token limit"); }
        options.max_expansions = 100;
        options.max_output_tokens = 100;
        options.max_expansion_depth = 4;
        options.max_expression_depth = 8;
        metapp::Preprocessor nested(options);
        try { nested.process("#define F(x) x\nF(F(F(F(F(1)))))"); check(false, "macro nesting limit"); }
        catch (const metapp::Error&) { check(true, "macro nesting limit"); }
        try { nested.process("#if (((((((((((1)))))))))))\n#endif"); check(false, "expression nesting limit"); }
        catch (const metapp::Error&) { check(true, "expression nesting limit"); }
        options.max_output_tokens = 8;
        metapp::Preprocessor growth(options);
        try { growth.process("#define D(x) x x x\nD(1 2 3)"); check(false, "replacement token limit"); }
        catch (const metapp::Error&) { check(true, "replacement token limit"); }
    }
    auto located = metapp::tokenize("a\\\nb c\nd", {"test.c", 4, 2});
    check(located[0].text == "ab" && located[1].location.line == 5 && located[2].location.line == 6, "spliced physical locations");
    auto leading_splice = metapp::tokenize("\\\n\\\nword", {"test.c", 2, 3});
    check(leading_splice[0].location.line == 4 && leading_splice[0].location.column == 1, "leading splice locations");
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
