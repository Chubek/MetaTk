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
    check(R"(\the\toks0 OK)","OK");
    check(R"(\toks255={OK}\the\toks255)","OK");
    check(R"(\toks0={\unknown}\def\unknown{OK}\the\toks0)","OK");
    check(R"(\toks0={A}\toks1=\toks0\toks0={B}\the\toks1\the\toks0)","AB");
    check(R"(\toks0={A}{\toks0={B}\the\toks0}\the\toks0)","BA");
    check(R"(\toks0={A}{\toks0={B}{\global\toks0={C}}}\the\toks0)","C");
    check(R"(\toksdef\words=0 \words={OK}\the\words)","OK");
    check(R"({\global\toksdef\words=0 }\words={OK}\the\words)","OK");
    check(R"(\toksdef\words=0 \let\saved=\words\def\words{BAD}\saved={OK}\the\saved)","OK");
    check(R"(\def\a{old}\toks0={\a}\edef\b{\the\toks0}\def\a{new}\b)","new");
    check(R"(\toks0={\unknown}\edef\b{\the\toks0}\def\unknown{OK}\b)","OK");
    check(R"(\toks0={\def\a#1{#1}}\edef\b{\the\toks0}\b\a{OK})","OK");
    check(R"(\toks0={A}\toks0=\expandafter{\the\toks0 B}\the\toks0)","AB");
    check(R"(\toks0={A}{\xdef\saved{\the\toks0}}\saved)","A");
    check(R"(\catcode`\~=13 \def~{A}\toks0={~}\catcode`\~=12 \the\toks0~)","A~");
    check(R"(\toksdef\r=0 \r={A}{\countdef\r=0 \r=2 \the\r}\the\r)","2A");
    for(auto source:{R"(\toks256={A})",R"(\toksdef X=0)",R"(\toks0=\count0)",R"(\toks0=A)",R"(\toks0={missing)",R"(\advance\toks0 by 1)",R"(\number\toks0)",R"(\dimen0=\toks0)",R"(\skip0=\toks0)"}){
        Engine engine;CHECK(errors(engine.compile(source)));
    }
    Engine engine;CHECK(!errors(engine.compileFragment(R"(\toks0={OK})")));
    CHECK(renderText(engine.compileFragment(R"(\the\toks0)"))=="OK\n\n");
    CHECK(renderText(engine.compile(R"(\the\toks0)"))=="");
    std::cout<<"Token register tests passed\n";
}
