#include "MetaTeX.h"
#include <iostream>
#include <stdexcept>
using namespace metatex;
#define CHECK(x) do {if(!(x))throw std::runtime_error("check failed at line "+std::to_string(__LINE__)+": " #x);} while(false)
bool errors(const Document& doc){for(const auto& d:doc.diagnostics)if(d.severity==Diagnostic::Severity::Error)return true;return false;}
void check(const std::string& source,const std::string& expected){
    Engine engine;auto doc=engine.compile(source);
    if(errors(doc))throw std::runtime_error(source+": "+doc.diagnostics.front().message);
    CHECK(doc.blocks.size()==1);CHECK(doc.blocks[0].kind==BlockKind::Paragraph);
    CHECK(doc.blocks[0].spans.size()==1);CHECK(doc.blocks[0].spans[0].kind==SpanKind::Text);
    if(doc.blocks[0].spans[0].text!=expected)throw std::runtime_error(source+": expected '"+expected+"', got '"+doc.blocks[0].spans[0].text+"'");
    CHECK(renderHTML(doc).find("<p>"+expected+"</p>")!=std::string::npos);
    auto pdf=renderPDF(doc);CHECK(pdf.starts_with("%PDF-1.4"));
    CHECK(pdf.find("("+expected+") Tj")!=std::string::npos);
}
int main(){
    check(R"(\the\skip0)","0.0pt");
    check(R"(\skip255=1pt plus 2pt minus 3pt\relax\the\skip255)","1.0pt plus 2.0pt minus 3.0pt");
    check(R"(\skip0=-1pt plus 2fil minus -3fill\relax\the\skip0)","-1.0pt plus 2.0fil minus -3.0fill");
    check(R"(\skip0=1pt PLUS .5filll MINUS .25pt\relax\the\skip0)","1.0pt plus 0.5filll minus 0.25pt");
    check(R"(\skip0=1pt plus 0fil minus 0fill\relax\the\skip0)","1.0pt");
    check(R"(\skip0=1pt plus 2fil minus 3pt\relax\skip1=-\skip0\relax\the\skip1)","-1.0pt plus -2.0fil minus -3.0pt");
    check(R"(\count0=2 \skip0=\count0 pt plus 3pt\relax\the\skip0)","2.0pt plus 3.0pt");
    check(R"(\dimen0=2pt \skip0=\dimen0 plus 3pt\relax\the\skip0)","2.0pt plus 3.0pt");
    check(R"(\skip0=2pt plus 3pt\relax\dimen0=.5\skip0\relax\the\dimen0)","1.0pt");
    check(R"(\skip0=2pt plus 3pt\relax\ifdim\skip0=2pt OK\else BAD\fi)","OK");
    check(R"(\skip0=2pt plus 3pt\relax\number\skip0)","131072");
    check(R"(\skipdef\gap=0 \gap=1pt plus 2pt\relax\the\gap)","1.0pt plus 2.0pt");
    check(R"(\skipdef\gap=0 \let\saved=\gap\def\gap{BAD}\saved=1pt\relax\the\saved)","1.0pt");
    check(R"({\global\skipdef\gap=0 }\gap=1pt\relax\the\gap)","1.0pt");
    check(R"(\skip0=1pt\relax{\skip0=2pt\relax\the\skip0}/\the\skip0)","2.0pt/1.0pt");
    check(R"(\skip0=1pt\relax{\skip0=2pt\relax{\global\skip0=3pt\relax}}\the\skip0)","3.0pt");
    check(R"(\skip0=1pt\relax\edef\saved{\the\skip0}\skip0=2pt\relax\saved)","1.0pt");
    check(R"(\skip0=1pt plus 2pt minus 3pt\relax\advance\skip0 by 2pt plus 3pt minus 4pt\relax\the\skip0)","3.0pt plus 5.0pt minus 7.0pt");
    check(R"(\skip0=1pt plus 2fil minus 3pt\relax\advance\skip0 by 2pt plus 4pt minus 1fill\relax\the\skip0)","3.0pt plus 2.0fil minus 1.0fill");
    check(R"(\skip0=1pt plus 2fil\relax\advance\skip0 by 2pt plus 1filll\relax\the\skip0)","3.0pt plus 1.0filll");
    check(R"(\skip0=1pt plus 2fil\relax\advance\skip0 by -\skip0\relax\the\skip0)","0.0pt");
    check(R"(\skip0=1pt plus 2fil minus 3pt\relax\multiply\skip0 by 3 \divide\skip0 by 2 \the\skip0)","1.5pt plus 3.0fil minus 4.5pt");
    check(R"(\skip0=1pt plus 2fil\relax\multiply\skip0 by -1 \the\skip0)","-1.0pt plus -2.0fil");
    check(R"(\skip0=1pt\relax{\global\advance\skip0 by 2pt\relax}\the\skip0)","3.0pt");
    check(R"(\skip0=1pt\relax{\global\multiply\skip0 by 2 }\the\skip0)","2.0pt");
    check(R"(\skip0=4pt\relax{\global\divide\skip0 by 2 }\the\skip0)","2.0pt");
    for(auto source:{R"(\skip256=1pt)",R"(\skipdef X=0)",R"(\skip0=1pt plus 1fillll)",R"(\skip0=1pt plus 16384fil)"}){
        Engine engine;CHECK(errors(engine.compile(source)));
    }
    for(auto operation:{R"(\multiply\skip0 by 2147483647 )",R"(\divide\skip0 by 0 )",R"(\advance\skip0 by 16383pt\relax)"}){
        Engine engine;auto doc=engine.compile(std::string(R"(\skip0=1pt plus 2fil\relax)")+operation+R"(\the\skip0)");
        CHECK(errors(doc));CHECK(renderText(doc)=="1.0pt plus 2.0fil\n\n");
    }
    Engine engine;CHECK(!errors(engine.compileFragment(R"(\skip0=1pt plus 2fil\relax)")));
    CHECK(renderText(engine.compileFragment(R"(\the\skip0)"))=="1.0pt plus 2.0fil\n\n");
    CHECK(renderText(engine.compile(R"(\the\skip0)"))=="0.0pt\n\n");
    std::cout<<"Glue register tests passed\n";
}
