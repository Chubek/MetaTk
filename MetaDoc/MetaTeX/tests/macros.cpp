#include "MetaTeX.h"
#include <iostream>
#include <stdexcept>
using namespace metatex;
#define CHECK(x) do {if(!(x))throw std::runtime_error("check failed at line "+std::to_string(__LINE__)+": " #x);} while(false)
bool errors(const Document& doc){for(const auto& d:doc.diagnostics)if(d.severity==Diagnostic::Severity::Error)return true;return false;}
void check(const char* source,const std::string& expected){
    Engine engine;auto doc=engine.compile(source);
    CHECK(!errors(doc));CHECK(doc.blocks.size()==1);
    CHECK(doc.blocks[0].kind==BlockKind::Paragraph);
    CHECK(doc.blocks[0].spans.size()==1);
    CHECK(doc.blocks[0].spans[0].kind==SpanKind::Text);
    CHECK(doc.blocks[0].spans[0].text==expected);
    CHECK(renderText(doc)==expected+"\n\n");
    CHECK(renderHTML(doc).find("<p>"+expected+"</p>")!=std::string::npos);
    auto pdf=renderPDF(doc);CHECK(pdf.starts_with("%PDF-1.4"));
    CHECK(pdf.find("("+expected+") Tj")!=std::string::npos);
}
int main(){
    check(R"(\def\swap#1,#2;{#2/#1}\swap first,second;)","second/first");
    check(R"(\def\f abc#1!{#1}\f abcOK!)","OK");
    check(R"(\def\f#1aba{#1}\f aababa)","aba");
    check(R"(\def\f#1;{#1}\f {a;b};)","a;b");
    check(R"(\def\f#1;{#1}\f {a}{b};)","ab");
    check(R"(\def\f#1\stop{#1}\f OK\stop)","OK");
    check(R"(\def\f#1 #2;{#2/#1}\f A B;)","B/A");
    check(R"(\def\f#1,#2{#2#1}\f A,{B})","BA");
    check(R"(\def\f#1;{#1}\let\g=\f\def\f{bad}\g OK;)","OK");
    check(R"(\def\f#1#{#1}\f A{B})","AB");
    check(R"(\def\f#{A}\f{B})","AB");
    check(R"({\gdef\f#1;{#1}}\f OK;)","OK");
    check(R"(\def\outer#1{\def\inner##1;{#1##1}}\outer A\inner B;)","AB");
    for(auto source:{R"(\def\f#2{x})",R"(\def\f#1#1{x})",R"(\def\f#1{#2})",R"(\def\f{#})",R"(\def\f#1;{#1}\f missing)",R"(\def\f abc{x}\f abd)",R"(\def\f#1;{#1}\f })"}){
        Engine engine;CHECK(errors(engine.compile(source)));
    }
    // Long overlapping prefixes exercise bounded, linear delimiter matching.
    Engine engine;std::string input="\\def\\f#1"+std::string(1000,'a')+"b{OK}\\f "+std::string(100000,'a')+"b";
    CHECK(renderText(engine.compile(input))=="OK\n\n");
    std::cout<<"Macro parameter tests passed\n";
}
