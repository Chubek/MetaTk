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
    check(R"(\the\dimen0/\dimen255=1.5pt \the\dimen255)","0.0pt/1.5pt");
    check(R"(\dimen0=1sp \the\dimen0/\number\dimen0)","0.00002pt/1");
    check(R"(\dimen0=0.5sp \the\dimen0)","0.0pt");
    check(R"(\dimen0=-.5pt \the\dimen0)","-0.5pt");
    check(R"(\dimen0=--1,25PT \the\dimen0)","1.25pt");
    check(R"(\dimen0=1in \the\dimen0)","72.26999pt");
    check(R"(\dimen0=1pc \the\dimen0)","12.0pt");
    check(R"(\dimen0=1bp \number\dimen0)","65781");
    check(R"(\dimen0=1cm \number\dimen0)","1864679");
    check(R"(\dimen0=1mm \number\dimen0)","186467");
    check(R"(\dimen0=1dd \number\dimen0)","70124");
    check(R"(\dimen0=1cc \number\dimen0)","841489");
    check(R"(\dimen0='10pt \the\dimen0)","8.0pt");
    check(R"(\dimen0="10sp \number\dimen0)","16");
    check(R"(\def\value{1.25}\def\unit{pt}\dimen0=\value\unit\relax\the\dimen0)","1.25pt");
    check(R"(\dimen0=3pt \dimen1=-.5\dimen0 \the\dimen1)","-1.5pt");
    check(R"(\dimen0=-3pt \dimen1=1.5\dimen0 \the\dimen1)","-4.5pt");
    check(R"(\count0=2 \dimen0=\count0 pt \the\dimen0)","2.0pt");
    check(R"(\count0=2 \dimen0=3\count0 \number\dimen0)","6");
    check(R"(\dimen0=1pt {\dimen0=2pt \the\dimen0}/\the\dimen0)","2.0pt/1.0pt");
    check(R"(\dimen0=1pt {\dimen0=2pt {\global\dimen0=3pt }}\the\dimen0)","3.0pt");
    check(R"(\dimen0=1pt {\global\dimen0=3pt \dimen0=4pt }\the\dimen0)","3.0pt");
    check(R"(\dimendef\size=0 \size=2pt \advance\size by .5pt \multiply\size by 3 \divide\size by 2 \the\size)","3.75pt");
    check(R"({\global\dimendef\size=0 }\size=2pt \the\size)","2.0pt");
    check(R"(\dimendef\size=0 \size=2pt \let\saved=\size\def\size{X}\the\saved)","2.0pt");
    check(R"(\dimendef\size=0 {\global\let\saved=\size}\saved=2pt \the\saved)","2.0pt");
    check(R"(\countdef\r=0 \r=3 {\dimendef\r=0 \r=2pt \the\r}/\the\r)","2.0pt/3");
    check(R"(\dimen0=2pt \edef\value{\the\dimen0}\dimen0=3pt \value)","2.0pt");
    check(R"(\ifdim1pt<2pt OK\else BAD\fi)","OK");
    check(R"(\ifdim1pt=65536sp OK\else BAD\fi)","OK");
    check(R"(\ifdim-1pt>0pt BAD\else OK\fi)","OK");
    check(R"(\iffalse\ifdim1pt=1pt BAD\fi\else OK\fi)","OK");
    check(R"(\edef\result{\ifdim2pt>1pt OK\else BAD\fi}\result)","OK");
    check(R"(\dimen0=1sp \multiply\dimen0 by -3 \divide\dimen0 by 2 \number\dimen0)","-1");
    check(R"(\dimen0=1pt {\global\advance\dimen0 by 2pt }\the\dimen0)","3.0pt");
    check(R"(\dimen0=1pt {\global\multiply\dimen0 by 2 }\the\dimen0)","2.0pt");
    check(R"(\dimen0=4pt {\global\divide\dimen0 by 2 }\the\dimen0)","2.0pt");
    check(R"(\dimen0=1073741823sp \the\dimen0)","16383.99998pt");
    for(auto source:{R"(\dimen256=1pt)",R"(\dimen-1=1pt)",R"(\dimendef X=0)",R"(\dimen0=1em)",R"(\dimen0=1)",R"(\ifdim1pt!2pt A\fi)"}){
        Engine engine;CHECK(errors(engine.compile(source)));
    }
    for(auto assignment:{R"(\dimen0=16384pt )",R"(\dimen0=999999999999999999999999pt )",R"(\multiply\dimen0 by 2147483647 )",R"(\divide\dimen0 by 0 )"}){
        Engine engine;auto doc=engine.compile(std::string(R"(\dimen0=1pt )")+assignment+R"(\the\dimen0)");
        CHECK(errors(doc));CHECK(renderText(doc)=="1.0pt\n\n");
    }
    // Verify printscaled/scandimen round-trips across rounding boundaries.
    for(int value:{0,1,2,3,32767,32768,32769,65535,65536,65537,1000001,1073741823,-1,-1073741823}){
        check("\\dimen0="+std::to_string(value)+R"(sp \edef\value{\the\dimen0}\dimen1=\value\relax\number\dimen1)",std::to_string(value));
    }
    Engine engine;CHECK(!errors(engine.compileFragment(R"(\dimen0=2pt )")));
    CHECK(renderText(engine.compileFragment(R"(\the\dimen0)"))=="2.0pt\n\n");
    CHECK(renderText(engine.compile(R"(\the\dimen0)"))=="0.0pt\n\n");
    std::cout<<"Dimension register tests passed\n";
}
