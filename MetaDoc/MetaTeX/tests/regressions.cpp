#include "MetaTeX.h"
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace metatex;
#define CHECK(x) do {if(!(x))throw std::runtime_error("check failed at line "+std::to_string(__LINE__)+": " #x);} while(false)
bool errors(const Document& doc){for(const auto& d:doc.diagnostics)if(d.severity==Diagnostic::Severity::Error)return true;return false;}
template<class F> void fails(F fn,const std::string& message){
    try {fn();}catch(const std::runtime_error& ex){CHECK(std::string(ex.what()).find(message)!=std::string::npos);return;}
    throw std::runtime_error("expected failure: "+message);
}
int main(int argc,char** argv){
    CHECK(argc==2);auto dir=std::filesystem::path(argv[1]);std::filesystem::create_directories(dir/"sub");
    Engine e;
    CHECK(renderText(e.compile(R"(\def\a{A}{\def\a{B}\gdef\a{C}\a}\a)"))=="CC\n\n");
    auto styled=e.compile(R"({\bfseries B}N {\itshape I}N \textbf{\rmfamily X}N)");
    CHECK(!errors(styled));auto html=renderHTML(styled);
    CHECK(html.find("<strong>B</strong>N")!=std::string::npos);
    CHECK(html.find("<em>I</em>N XN")!=std::string::npos);
    for(auto source:{R"(\textbf{\textit}Tail)",R"(\textbf{\iftrue X}Tail)",R"(\textbf{\csname}Tail)",R"(\textbf{$x}Tail)"}){
        auto doc=e.compile(source);CHECK(errors(doc));CHECK(renderText(doc).find("Tail")!=std::string::npos);
    }
    CHECK(renderHTML(e.compile(R"($\alpha x$)")).find("<mo>α</mo><mi>x</mi>")!=std::string::npos);
    CHECK(renderText(e.compile(R"(\ifnum -2 < +3 yes\else no\fi)"))=="yes\n\n");
    CHECK(errors(e.compile(R"(\ifnum 1 ! 2 x\fi)")));
    e.registerCommand("optional",[](Engine& x){x.addText(x.readOptionalText());});
    CHECK(renderText(e.compile(R"(\optional[{a]b}]Tail)"))=="{a]b}Tail\n\n");
    e.registerCommand("heading",[](Engine& x){x.heading(1);});
    e.compile(R"(\heading{One}\heading{Two})");
    e.registerCommand("label",[](Engine& x){x.label(x.readTextArgument());});
    CHECK(e.compile(R"(\label{x})").references.at("x")=="1");
    e.registerCommand("install",[](Engine& x){x.registerCommand("temporary",[](Engine& y){y.addText("local");});});
    CHECK(renderText(e.compile(R"(\install\temporary)"))=="local\n\n");
    CHECK(errors(e.compile(R"(\temporary)")));
    e.registerCommand("overflow",[](Engine& x){x.setCounter("n",std::numeric_limits<int>::max());x.stepCounter("n");});
    CHECK(errors(e.compile(R"(\overflow)")));CHECK(e.counter("n")==std::numeric_limits<int>::max());
    e.registerCommand("underflow",[](Engine& x){x.setCounter("n",std::numeric_limits<int>::min());x.addToCounter("n",-1);});
    CHECK(errors(e.compile(R"(\underflow)")));CHECK(e.counter("n")==std::numeric_limits<int>::min());

    writeFile(dir/"sub"/"one.tex",R"(A\input{two}B\endinput ignored)");
    writeFile(dir/"sub"/"two.tex","C\n\\undefined\n");
    auto included=e.compile(R"(\input{sub/one}End)",dir/"main.tex");
    CHECK(renderText(included).find("BEnd")!=std::string::npos);
    CHECK(included.diagnostics.size()==1);CHECK(included.diagnostics[0].file==(dir/"sub"/"two.tex").string());
    CHECK(included.diagnostics[0].line==2);
    writeFile(dir/"loop.tex",R"(\input{loop})");
    auto recursive=e.compile(R"(\input{loop}End)",dir/"main.tex");
    CHECK(errors(recursive));CHECK(recursive.diagnostics[0].message=="input nesting limit exceeded");
    CHECK(renderText(recursive)=="End\n\n");
    writeFile(dir/"sub"/"two.tex",R"(C\bye ignored)");
    CHECK(renderText(e.compile(R"(\input{sub/one}End)",dir/"main.tex"))=="AC\n\n");
    fails([&]{readFile(dir/"loop.tex",3);},"input size limit");
    fails([&]{e.compile(R"(\def\a{\textbf{\a}}\a)");},"nested execution limit");
    fails([&]{e.compile(std::string(1024,'{'));},"group nesting limit");
    CHECK(renderText(e.compile("recovered"))=="recovered\n\n");
    fails([&]{e.compile(R"(\def\a{\a}\a)");},"processing budget");
    CHECK(renderText(e.compile("recovered"))=="recovered\n\n");
    fails([]{renderMathML(std::string(200,'{')+"x"+std::string(200,'}'));},"math nesting limit");
    for(const std::string invalid:{std::string("\xC0\xAF"),std::string("\xED\xA0\x80"),std::string("\xF4\x90\x80\x80"),std::string("\xE2\x82"),std::string("\0",1)})
        fails([&]{Lexer::scan(invalid);},invalid[0]==0?"NUL":"UTF-8");
    auto tokens=Lexer::scan("\\\r\nX");CHECK(tokens.size()==2);CHECK(tokens[1].line==2&&tokens[1].column==1);
    CHECK(renderText(e.compile("α 世界"))=="α 世界\n\n");
    std::cout<<"Core regression tests passed\n";
}
