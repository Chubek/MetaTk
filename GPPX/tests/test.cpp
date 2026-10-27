#include "GPPX.hpp"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
namespace fs=std::filesystem;
static int tests=0;
static void check(const std::string& name,const std::string& source,const std::string& expected){
 gppx::Engine e;std::string got;
 try{got=e.process(source);}catch(const std::exception& ex){std::cerr<<"FAIL "<<name<<" exception: "<<ex.what()<<'\n';std::exit(1);}
 if(got!=expected){std::cerr<<"FAIL "<<name<<"\nexpected: ["<<expected<<"]\nactual:   ["<<got<<"]\n";std::exit(1);}++tests;
}
static void throws(const std::string& name,const std::string& source){gppx::Engine e;try{(void)e.process(source);}catch(const gppx::Error&){++tests;return;}std::cerr<<"FAIL missing error: "<<name<<'\n';std::exit(1);}
static void other(const std::string& name,std::function<void()> fn){try{fn();++tests;}catch(const std::exception& ex){std::cerr<<"FAIL "<<name<<": "<<ex.what()<<'\n';std::exit(1);}}
int main(){
 check("passthrough","hello\nworld\n","hello\nworld\n");
 check("object macro","#define WORLD Earth\nHello WORLD!\n","Hello Earth!\n");
 check("recursive macro","#define A B\n#define B C\nA\n","C\n");
 check("self recursion","#define A A\nA\n","A\n");
 check("mutual recursion","#define A B\n#define B A\nA\n","A\n");
 check("function","#define ADD(x,y) ((x)+(y))\nADD(4,5)\n","((4)+(5))\n");
 check("function nested commas","#define WRAP(x) x\nWRAP((1,2))\n","(1,2)\n");
 check("empty function argument","#define WRAP(x) [x]\nWRAP()\n","[]\n");
 check("block comment delimiters","#repeat 1\n/*\n#endrepeat\n*/\nx\n#endrepeat\n","/*\n#endrepeat\n*/\nx\n");
 check("raw in repeat","#repeat 1\n#raw\n#endrepeat\n#endraw\n#endrepeat\n","#endrepeat\n");
 check("inactive comments","#if 0\n/*\n#endif\n*/\n#else\nYES\n#endif\n","YES\n");
 check("unsigned negation","#eval -(-9223372036854775807 - 1)\n","-9223372036854775808\n");
 check("variadics","#define F(x,...) x + __VA_ARGS__\nF(1,2,3)\n","1 + 2, 3\n");
 check("stringification","#define STR(x) #x\nSTR(abc)\n","\"abc\"\n");
 check("token paste","#define JOIN(a,b) a ## b\nJOIN(foo,bar)\n","foobar\n");
 check("string protection","#define X red\n\"X\" X 'X'\n","\"X\" red 'X'\n");
 check("expand incomplete literal","#define X red\n#syntax expand_strings true\n\"X","\"red");
 check("expand escaped quote","#define X red\n#syntax expand_strings true\n\"X\\\"","\"red\\\"");
 check("comment protection","#define X red\nX // X\n/* X\nX */ X\n","red // X\n/* X\nX */ red\n");
 check("conditional","#define A 8\n#if defined(A) && A >= 8\nYES\n#else\nNO\n#endif\n","YES\n");
 check("elif","#if 0\nA\n#elif 1\nB\n#else\nC\n#endif\n","B\n");
 check("nested conditional","#if 0\n#if 1\nX\n#endif\n#else\nY\n#endif\n","Y\n");
 check("ifdef","#define X 1\n#ifdef X\nOK\n#endif\n#ifndef Q\nYES\n#endif\n","OK\nYES\n");
 check("short circuit","#if 1 || (1/0)\nA\n#endif\n#if 0 && (1/0)\nB\n#endif\n","A\n");
 check("precedence","#eval 1+2*3\n#eval (1+2)*3\n#eval 1<<3\n","7\n9\n8\n");
 check("ternary","#eval 1 ? 10 : 1/0\n#eval 0 ? 1/0 : 20\n#eval (1 ? 2 : 3) + 4\n","10\n20\n6\n");
 check("ifmatch regex","#ifmatch regex \"bc\" => \"abcd\"\nMATCH\n#else\nMISS\n#endif\n","MATCH\n");
 check("elifmatch regex","#if 0\nBAD\n#elifmatch regex \"z+\" => \"zzz\"\nOK\n#else\nBAD\n#endif\n","OK\n");
 check("conditional skipped block","#if 0\n#macro F(x)\n#if 1\nBROKEN\n#endif\n#endmacro\n#endif\nSAFE\n","SAFE\n");
 check("for variable restore","#define X original\n#for X in 1..2\nX\n#endfor\nX\n","1\n2\noriginal\n");
 check("nested output size","#syntax max_output 6\n#repeat 2\nAB\n#endrepeat\n","AB\nAB\n");
 check("nested blocks","#repeat 2\n#repeat 2\nx\n#endrepeat\n#endrepeat\n","x\nx\nx\nx\n");
 check("for ascending","#for I in 1..3\nI\n#endfor\n","1\n2\n3\n");
 check("for descending","#for I in 3..1\nI\n#endfor\n","3\n2\n1\n");
 check("block macros","#macro TWICE(x)\nx x\n#endmacro\nTWICE(hi)\n","hi hi\n\n");
 check("raw","#define X Y\n#raw\nX\n#define A B\n#endraw\nX\n","X\n#define A B\nY\n");
 check("push pop","#define X old\n#push_macro X\n#define X new\nX\n#pop_macro X\nX\n","new\nold\n");
 check("rewrite regex","#rewrite \"cat\" => \"dog\"\ncat\n","dog\n");
 check("syntax percent","#syntax directive %\n%define X Y\nX\n","Y\n");
 check("syntax template","#syntax profile template\n@define X Y\n{{X}}\n","Y\n");
 check("syntax template nested","#syntax profile template\n@define X green\n@define WRAP(x) [x]\n{{WRAP({{X}})}}\n","[green]\n");
 check("syntax explicit quote safety","#syntax macros mustache\n#define X green\n\"{{X}}\" {{X}}\n","\"{{X}}\" green\n");
 check("syntax nested dollar","#syntax macros dollar\n#define X okay\n#define WRAP(x) [x]\n$(WRAP($(X)))\n","[okay]\n");
 check("explicit dollar","#syntax macros dollar\n#define X Y\nX $(X)\n","X Y\n");
 check("builtin line","__LINE__\n__LINE__\n","1\n2\n");
 check("counter","__COUNTER__ __COUNTER__\n","0 1\n");
 check("warning","#warning a gentle warning\nGOOD\n","GOOD\n");
 check("sourceless","","");
 throws("stray endif","#endif\n");
 throws("unmatched if","#if 1\nhi\n");
 throws("duplicate else","#if 1\n#else\n#else\n#endif\n");
 throws("division by zero","#eval 3/0\n");
 throws("invalid name","#define 23=24\n");
 throws("missing block","#raw\nfoo\n");
 throws("unknown directive","#foobarbaz\n");
 throws("unbalanced invocation","#define F(x) x\nF(a,b)\n");
 throws("output limit","#syntax max_output 5\n#repeat 2\nAB\n#endrepeat\n");
 throws("assert failure","#assert 0\n");
 throws("negative resource limit","#syntax max_output -1\n");
 throws("trailing resource limit","#syntax max_depth 10garbage\n");
 throws("bad boolean","#syntax strict maybe\n");
 throws("block nesting limit","#syntax max_depth 2\n#repeat 1\n#repeat 1\nx\n#endrepeat\n#endrepeat\n");
 other("direct function call",[]{gppx::Engine e;e.define("WRAP(x)","[x]");e.set("profile","template");if(e.call("WRAP",{"a,b"})!="[a,b]")throw std::runtime_error("argument boundaries lost");});
 other("callback diagnostic location",[]{gppx::Engine e;e.registerDirective("fail",[](std::string_view)->std::string{throw std::runtime_error("callback error");});try{e.process("ok\n#fail\n","source.gppx");}catch(const gppx::Error& ex){if(std::string(ex.what())!="source.gppx:2: callback error")throw;return;}throw std::runtime_error("missing failure");});
 other("reentrant processing limit",[]{gppx::Engine e;e.set("max_depth","4");e.registerDirective("recurse",[&](std::string_view){return e.process("#recurse\n");});try{e.process("#recurse\n");}catch(const gppx::Error&){if(e.process("ok")!="ok")throw std::runtime_error("state not restored");return;}throw std::runtime_error("missing failure");});
 other("binary string embed",[]{gppx::Engine e;e.registerReader(".gppx",[](const fs::path&){return std::string("A\0" "7\xFF",4);});auto f=(fs::path(GPPX_TEST_DIR)/"examples/include/plain.gppx").string();auto result=e.process("#embed \""+f+"\" string\n");if(result!="\"A\\0007\\377\"\n")throw std::runtime_error(result);});
 throws("non-short-circuited div0","#if 1 && 1/0\nX\n#endif\n");
 other("file include once",[]{gppx::Engine e;auto root=fs::path(GPPX_TEST_DIR)/"examples/include";e.includePath(root);std::string output=e.process("#include \"header.gppx\"\n#include \"header.gppx\"\n","<test>");if(output!="Header 12\n")throw std::runtime_error("actual: "+output);});
 other("file include",[]{gppx::Engine e;auto p=fs::path(GPPX_TEST_DIR)/"examples/include/plain.gppx";auto output=e.process("#include \""+p.string()+"\"\n");if(output!="PLAIN\n")throw std::runtime_error(output);});
 other("import suppresses ordinary include",[]{gppx::Engine e;auto p=(fs::path(GPPX_TEST_DIR)/"examples/include/plain.gppx").string();auto output=e.process("#import \""+p+"\"\n#include \""+p+"\"\n");if(output!="PLAIN\n")throw std::runtime_error(output);});
 other("failed import may be retried",[]{gppx::Engine e;auto p=(fs::path(GPPX_TEST_DIR)/"examples/include/plain.gppx").string();e.registerReader(".gppx",[](const fs::path&)->std::string{throw gppx::Error("read failed");});try{e.process("#import \""+p+"\"\n");}catch(const gppx::Error&){}e.registerReader(".gppx",[](const fs::path&){return "retry\n";});if(e.process("#import \""+p+"\"\n")!="retry\n")throw std::runtime_error("import cache poisoned");});
 other("direct call recursion limit",[]{gppx::Engine e;e.set("max_depth","4");e.registerMacro("RECURSE",[&](const std::vector<std::string>&){return e.call("RECURSE",{});});try{e.call("RECURSE",{});}catch(const gppx::Error&){e.define("OK()","done");if(e.call("OK",{})!="done")throw std::runtime_error("call depth not restored");return;}throw std::runtime_error("missing failure");});
 other("embedded resource",[]{gppx::Engine e;auto f=(fs::path(GPPX_TEST_DIR)/"examples/include/plain.gppx").string();auto result=e.process("#embed \""+f+"\" hex\n");if(result!="504C41494E0A\n")throw std::runtime_error(result);});
 other("custom callback",[]{gppx::Engine e;e.registerDirective("greet",[](std::string_view name){return "Welcome "+std::string(name);});e.registerMacro("UP",[](const std::vector<std::string>& args){return args.at(0)+"!";});auto out=e.process("#greet friend\nUP(hey)\n");if(out!="Welcome friend\nhey!\n")throw std::runtime_error(out);});
 other("reader/sink/pattern/syntax",[]{gppx::Engine e;std::string store;e.registerReader(".fake",[](const fs::path&){return "CONTENTS";});e.registerSink("capture",[&](std::string_view s){store=std::string(s);});e.registerPMatch("prefix",[](auto a,auto b){return b.substr(0,a.size())==a;});e.registerSyntax("custom",[&](std::string_view val){e.define("CUSTOM",std::string(val));});e.set("custom","YES");if(e.read("whatever.fake")!="CONTENTS"||!e.match("prefix","ab","abc"))throw std::runtime_error("reader/matcher");if(e.process("#sink capture CUSTOM\n")!=""||store!="YES")throw std::runtime_error("sink");});
 other("C ABI",[]{GPPX_State* s=gppx_create();if(!s)throw std::runtime_error("create");if(gppx_define(s,"NAME","gppx"))throw std::runtime_error(gppx_last_error(s));char* result=nullptr;int rc=gppx_process(s,"NAME\n","<c>",&result);std::string got=result?result:"";gppx_free(result);gppx_destroy(s);if(rc||got!="gppx\n")throw std::runtime_error(got);});
#ifdef GPPX_WEBBER_PATH
 other("shared plugin",[]{gppx::Engine e;e.loadPlugin(GPPX_WEBBER_PATH);auto got=e.process("#wbr_upper hello world\nWBR_UPPER(foo) WBR_VERSION()\n");if(got!="HELLO WORLD\nFOO 1\n")throw std::runtime_error(got);if(!e.match("wbr_contains","ell","hello"))throw std::runtime_error("match");
   if(e.process("#ifmatch wbr_contains \"ell\" => \"hello\"\nYES\n#endif\n")!="YES\n")throw std::runtime_error("ifmatch plugin");});
 other("assembler embed",[]{gppx::Engine e;e.loadPlugin(GPPX_WEBBER_PATH);auto src=(fs::path(GPPX_TEST_DIR)/"examples/webber/sample.wbr").string();auto got=e.process("#embed \""+src+"\" wbr_asm\n");if(got!="0x10, 0x04, 0x10, 0x05, 0x20, 0x30, 0xFF\n")throw std::runtime_error(got);});
 other("assembler extended",[]{
    gppx::Engine e;e.loadPlugin(GPPX_WEBBER_PATH);
    auto src=(fs::path(GPPX_TEST_DIR)/"examples/webber/extended.wbr").string();
    auto hex=e.process("#embed \""+src+"\" wbr_hex\n");
    // PUSH 10 / DUP / ADD / JNZ forward / ASCII / ASCIZ / ALIGN / WORD / HALT
    if(hex!="100A1120421200413B42430A000000001200FF\n") throw std::runtime_error(hex);
 });
#endif
 std::cout<<"PASS "<<tests<<" GPPX tests\n";return 0;
}
