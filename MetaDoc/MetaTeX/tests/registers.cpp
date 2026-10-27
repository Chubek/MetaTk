#include "MetaTeX.h"
#include <iostream>
#include <stdexcept>
using namespace metatex;
#define CHECK(x) do {if(!(x))throw std::runtime_error("check failed at line "+std::to_string(__LINE__)+": " #x);} while(false)
bool errors(const Document& doc){for(const auto& d:doc.diagnostics)if(d.severity==Diagnostic::Severity::Error)return true;return false;}
void check(const char* source,const std::string& expected){
    Engine engine;auto doc=engine.compile(source);
    if(errors(doc))throw std::runtime_error(std::string(source)+": "+doc.diagnostics.front().message);
    CHECK(doc.blocks.size()==1);CHECK(doc.blocks[0].kind==BlockKind::Paragraph);
    CHECK(doc.blocks[0].spans.size()==1);CHECK(doc.blocks[0].spans[0].kind==SpanKind::Text);
    CHECK(doc.blocks[0].spans[0].text==expected);
    CHECK(renderHTML(doc).find("<p>"+expected+"</p>")!=std::string::npos);
    auto pdf=renderPDF(doc);CHECK(pdf.starts_with("%PDF-1.4"));
    CHECK(pdf.find("("+expected+") Tj")!=std::string::npos);
}
int main(){
    check(R"(\the\count0/\count255=42 \the\count255)","0/42");
    check(R"(\count0=1 {\count0=2 \the\count0}/\the\count0)","2/1");
    check(R"(\count0=1 {\count0=2 {\global\count0=3 }\the\count0}/\the\count0)","3/3");
    check(R"(\count0=1 {\global\count0=3 \count0=4 \the\count0}/\the\count0)","4/3");
    check(R"(\countdef\n=10 \n=7 \advance\n by 5 \multiply\n by -3 \divide\n by 5 \the\n)","-7");
    check(R"(\count0=5 \advance\count0 2 \multiply\count0 BY 3 \divide\count0 2 \number\count0)","10");
    check(R"(\count0=5 \count1=-\count0 \the\count1)","-5");
    check(R"(\count0=7 \edef\saved{\the\count0}\count0=9 \saved/\the\count0)","7/9");
    check(R"(\count0=7 \ifnum\count0>6 OK\else BAD\fi)","OK");
    check(R"(\countdef\n=0 \n=7 {\countdef\n=1 \n=9 \the\n}/\the\n)","9/7");
    check(R"({\global\countdef\n=0 }\n=7 \the\n)","7");
    check(R"(\countdef\n=0 \n=7 {\def\n{X}\n}/\the\n)","X/7");
    check(R"({\global\def\foo{OK}}\foo)","OK");
    check(R"(\def\a{OK}{\global\edef\b{\a}}\def\a{BAD}\b)","OK");
    check(R"(\count0=2 {\global\advance\count0 by 3 }\the\count0)","5");
    check(R"(\count0=2 {\global\multiply\count0 by 3 }\the\count0)","6");
    check(R"(\count0=9 {\global\divide\count0 by 2 }\the\count0)","4");
    for(auto source:{R"(\count256=1)",R"(\count-1=1)",R"(\global X)",R"(\countdef X=1)",R"(\the X)"}){
        Engine engine;CHECK(errors(engine.compile(source)));
    }
    for(auto source:{R"(\count0=2147483647 \advance\count0 by 1 \the\count0)",R"(\count0=2147483647 \multiply\count0 by 2 \the\count0)",R"(\count0=2147483647 \divide\count0 by 0 \the\count0)"}){
        Engine engine;auto doc=engine.compile(source);CHECK(errors(doc));CHECK(renderText(doc)=="2147483647\n\n");
    }
    Engine engine;CHECK(!errors(engine.compileFragment(R"(\count0=42 )")));
    CHECK(renderText(engine.compileFragment(R"(\the\count0)"))=="42\n\n");
    CHECK(renderText(engine.compile(R"(\the\count0)"))=="0\n\n");
    std::cout<<"Integer register tests passed\n";
}
