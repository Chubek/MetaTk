#include "MetaTeX.h"
#include <iostream>
#include <stdexcept>
using namespace metatex;
extern "C" void metatex_register_plugin(Engine*);
#define CHECK(x) do {if(!(x))throw std::runtime_error("check failed at line "+std::to_string(__LINE__)+": " #x);} while(false)
bool errors(const Document& doc){for(const auto& d:doc.diagnostics)if(d.severity==Diagnostic::Severity::Error)return true;return false;}
void check(const char* source,const std::string& expected,bool latex=false){
    Engine engine;if(latex)metatex_register_plugin(&engine);
    auto doc=engine.compile(source);
    if(errors(doc))throw std::runtime_error(std::string(source)+": "+doc.diagnostics.front().message);
    CHECK(doc.blocks.size()==1);CHECK(doc.blocks[0].kind==BlockKind::Paragraph);
    CHECK(doc.blocks[0].spans.size()==1);CHECK(doc.blocks[0].spans[0].kind==SpanKind::Text);
    CHECK(doc.blocks[0].spans[0].text==expected);
    CHECK(renderHTML(doc).find("<p>"+expected+"</p>")!=std::string::npos);
    auto pdf=renderPDF(doc);CHECK(pdf.starts_with("%PDF-1.4"));
    CHECK(pdf.find("("+expected+") Tj")!=std::string::npos);
}
int main(){
    check(R"(^^41^^42^^43)","ABC");
    check(R"(\def\fo^^6f^^7bOK^^7d\foo)","OK");
    check(R"(^^5e^61)","a");
    check(R"(^^4A)","tA");
    check(R"(A^^00B)","AB");
    check("A^^Mignored\nB","A B");
    check("A^^25ignored^^0d also ignored\nB","AB");
    check(R"(\catcode`\^=12 ^^41)","^^41");
    check(R"(\catcode`\!=7 !!41)","A");
    check(R"(\catcode`\~=13 \def~{OK}^^7e)","OK");
    check(R"(\def\a{A}{\def\a{B}\aftergroup\a\a})","BA");
    check(R"({\aftergroup A\aftergroup B}C)","ABC");
    check(R"({\aftergroup A{\aftergroup B}C}D)","BCAD");
    check(R"(\aftergroup A B)","B");
    check(R"(\catcode`\~=13 \def~{A}{\aftergroup~\catcode`\~=12 })","A");
    check(R"(\newcommand{\pair}[2][left]{#1/#2}\pair{right})","left/right",true);
    check(R"(\newcommand{\pair}[2][left]{#1/#2}\pair[other]{right})","other/right",true);
    check(R"(\newcommand{\pair}[2][left]{#1/#2}\pair[]{right})","/right",true);
    check(R"(\def\value{old}\newcommand{\pair}[1][\value]{#1}\def\value{new}\pair)","new",true);
    check(R"(\newcommand{\a}[1][A]{#1}{\renewcommand{\a}[1][B]{#1}\a}\a)","BA",true);
    check(R"(\providecommand{\a}[1][A]{#1}\providecommand{\a}[1][B]{#1}\a)","A",true);
    Engine engine;metatex_register_plugin(&engine);
    CHECK(errors(engine.compile(R"(\newcommand{\a}[0][bad]{#1})")));
    CHECK(errors(engine.compile(R"(\newcommand{\a}[1][x]{#1}\a[missing)")));
    CHECK(errors(engine.compile(R"({\aftergroup})")));
    CHECK(errors(engine.compile(R"(\aftergroup)")));
    auto tokens=Lexer::scan("^^41\\bad\r\nX");
    CHECK(tokens[0].offset==0&&tokens[1].column==5);
    CHECK(tokens.back().line==2&&tokens.back().column==1);
    std::cout<<"Source and macro feature tests passed\n";
}
