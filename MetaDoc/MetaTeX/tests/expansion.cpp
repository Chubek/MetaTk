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
    check(R"(\def\a{A}\edef\b{\a}\def\a{B}\b\a)","AB");
    check(R"(\def\a{A}{\def\a{B}\xdef\b{\a}}\b\a)","BA");
    check(R"(\def\a{A}\edef\b{\noexpand\a}\def\a{B}\b)","B");
    check(R"(\edef\b{\noexpand\unknown}\def\unknown{OK}\b)","OK");
    check(R"(\def\a{BAD}\noexpand\a OK)","OK");
    check(R"(\def\a{A}\def\take#1{[#1]}\expandafter\take\a)","[A]");
    check(R"(\expandafter\def\csname foo\endcsname{OK}\foo)","OK");
    check(R"(\def\name{foo}\def\foo{OK}\csname\name\endcsname)","OK");
    check(R"(\expandafter\def\csname foo bar\endcsname{A}\def\foobar{B}\csname foo bar\endcsname\foobar)","AB");
    check(R"(\noexpand\def\a{OK}\a)","OK");
    check(R"(\csname unknown\endcsname\ifdefined\unknown OK\else BAD\fi)","OK");
    check(R"(\def\id#1{#1}\edef\foo#1{\id{#1}}\foo{OK})","OK");
    check(R"(\edef\foo{\iftrue OK\else BAD\fi}\foo)","OK");
    check(R"(\def\n{42}\edef\foo{\ifnum\n>41 OK\else BAD\fi}\foo)","OK");
    check(R"(\number--42/\number'17/\number"FF/\number`A)","42/15/255/65");
    check(R"(\romannumeral1994/\romannumeral0/\romannumeral-1)","mcmxciv//");
    check(R"(\edef\n{\number42}\n)","42");
    check(R"(\string#\string$\string{\string})","#${}");
    check(R"(\edef\a{\def\noexpand\b{OK}}\a\b)","OK");
    for(auto source:{R"(\edef\a{\undefined})",R"(\edef\a{\a})",R"(\number2147483648)",R"(\number X)",R"(\expandafter)",R"(\noexpand)",R"(\csname\relax\endcsname)"}){
        Engine engine;CHECK(errors(engine.compile(source)));
    }
    Engine engine;
    try {engine.compile(R"(\def\a{\a}\edef\b{\a})");throw std::logic_error("runaway accepted");}
    catch(const std::runtime_error& error){CHECK(std::string(error.what()).find("processing budget")!=std::string::npos);}
    CHECK(renderText(engine.compile("recovered"))=="recovered\n\n");
    engine.registerCommand("number",[](Engine& e){e.addText("custom");});
    CHECK(renderText(engine.compile(R"(\number)"))=="custom\n\n");
    CHECK(renderText(engine.compile(R"(\edef\a{\number}\a)"))=="custom\n\n");
    std::cout<<"Expansion scanner tests passed\n";
}
