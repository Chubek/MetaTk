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
int main(int argc,char** argv){
    CHECK(argc==2);auto directory=std::filesystem::path(argv[1]);std::filesystem::create_directories(directory);
    check(R"(\catcode`\@=11 \def\foo@bar{OK}\foo@bar)","OK");
    check(R"(\catcode`\!=0 !def!foo{OK}!foo)","OK");
    check(R"(\catcode`\[=1 \catcode`\]=2 \def\foo[OK]\foo)","OK");
    check(R"(\catcode`\!=6 \def\foo!1{!1}\foo{OK})","OK");
    check(R"(\catcode`\~=13 \def~{OK}~)","OK");
    check(R"(\catcode`\~=13 \def~{A}\def\~{B}~\~)","AB");
    check(R"(\def\literal{~}\catcode`\~=13 \def~{A}\literal~)","~A");
    check(R"(\catcode`\~=13~\def~{A}~)","~A");
    check(R"(\catcode`\~=13 \def~{A}\def\saved{~}\catcode`\~=12 \saved~)","A~");
    check(R"(\catcode`\!=9 A!B)","AB");
    check(R"(\catcode`\!=10 A!!B)","A B");
    check("\\catcode`\\!=14 A! ignored\nB","AB");
    check("\\catcode`\\!=5 A! ignored\nB","A B");
    check(R"(\catcode`\%=12 A%B)","A%B");
    check(R"({\catcode`\~=13 \def~{A}~}~)","A~");
    check(R"({\catcode`\~=13 {\global\catcode`\~=12 }}~)","~");
    check(R"({\global\catcode`\~=13 \catcode`\~=12 }\def~{OK}~)","OK");
    check(R"(\iftrue\catcode`\~=13 \def~{OK}~\else BAD\fi)","OK");
    check(R"(\iffalse BAD\else\catcode`\~=13 \def~{OK}~\fi)","OK");
    check(R"(\def\yes{\iftrue}\def\stop{\fi}\yes OK\stop)","OK");
    check(R"(\catcode`\~=13 \def~{A}\edef\saved{\noexpand~}\def~{B}\saved)","B");
    check(R"(\catcode`\~=13 \def~{A}\string~)","~");
    check(R"(\the\catcode`\@/\catcode`\@=11 \number\catcode`\@)","12/11");
    check("A% ignored\n   B","AB");
    Engine engine;
    CHECK(!errors(engine.compileFragment(R"(\catcode`\~=13 \def~{OK})")));
    CHECK(renderText(engine.compileFragment("~"))=="OK\n\n");
    CHECK(renderText(engine.compile("~"))=="~\n\n");
    writeFile(directory/"included.tex",R"(\catcode`\~=13 \def~{OK}~)");
    auto included=engine.compile(R"(\input{included}~)",directory/"main.tex");
    CHECK(!errors(included));CHECK(renderText(included)=="OKOK\n\n");
    writeFile(directory/"included.tex",R"(~)");
    included=engine.compile(R"(\catcode`\~=13 \def~{OK}\input{included}~)",directory/"main.tex");
    CHECK(!errors(included));CHECK(renderText(included)=="OKOK\n\n");
    for(auto source:{R"(\catcode256=1)",R"(\catcode65=16)",R"(\catcode65=-1)",R"(\iftrue)",R"(\iffalse X)",R"(\fi)"})CHECK(errors(engine.compile(source)));
    bool invalid=false;
    try {engine.compile(R"(\catcode`\!=15 !)");}catch(const std::runtime_error& e){invalid=std::string(e.what()).find("invalid category-code")!=std::string::npos;}
    CHECK(invalid);CHECK(renderText(engine.compile("recovered"))=="recovered\n\n");
    // Letter and other-character tokens with the same spelling are distinct delimiters.
    CHECK(errors(engine.compile(R"(\catcode`\@=11 \def\f#1@{#1}\catcode`\@=12 \f X@)")));
    std::cout<<"Incremental category-code tests passed\n";
}
