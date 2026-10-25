#include "MetaTeX.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace metatex;
#define CHECK(x) do {if(!(x))throw std::runtime_error("check failed at line "+std::to_string(__LINE__)+": " #x);} while(false)
bool failed(const Document& doc){for(const auto& d:doc.diagnostics)if(d.severity==Diagnostic::Severity::Error)return true;return false;}
void output(const Document& doc,const std::string& text,SpanKind kind=SpanKind::Text){
    for(const auto& d:doc.diagnostics)std::cerr<<d.message<<'\n';
    CHECK(!failed(doc));CHECK(doc.blocks.size()==1);CHECK(doc.blocks[0].spans.size()==1);
    CHECK(doc.blocks[0].spans[0].kind==kind);CHECK(doc.blocks[0].spans[0].text==text);
    CHECK(renderText(doc)==text+"\n\n");
    CHECK(renderHTML(doc).find(kind==SpanKind::Bold?"<strong>"+text+"</strong>":"<p>"+text+"</p>")!=std::string::npos);
    CHECK(renderPDF(doc).find("("+text+") Tj")!=std::string::npos);
}
int main(int argc,char** argv){
    CHECK(argc==2);std::filesystem::path dir=argv[1];std::filesystem::create_directories(dir);
    auto file=dir/"source.tex";
    Engine engine;
    output(engine.compile(R"(\mmlcommand{\shout}[1]{String.upper arg1}\shout{hello})"),"HELLO");
    output(engine.compile(R"(\mmlcommand{\bold}[1]{"\\textbf{" ^ arg1 ^ "}"}\bold{hello})"),"hello",SpanKind::Bold);
    output(engine.compile(R"(\mmlcommand{\printed}{IO.print "hello"}\printed)"),"hello");
    writeFile(dir/"formula.mml","val n := 10 % 3\nString.of_int n\n");
    output(engine.compile(R"(\mmlcommandfile{\formula}{formula.mml}\formula)",file),"1");
    writeFile(dir/"emit.sh","#!/bin/sh\nprintf '%s' '\\textbf{fromscript}'\n");
    std::filesystem::permissions(dir/"emit.sh",std::filesystem::perms::owner_all);
    output(engine.compile(R"(\Extern{emit.sh})",file),"fromscript",SpanKind::Bold);
    writeFile(dir/"argument.sh","#!/bin/sh\nprintf '%s' \"$1\" >&3\n");
    std::filesystem::permissions(dir/"argument.sh",std::filesystem::perms::owner_all);
    output(engine.compile(R"(\externcommand{\echo}[1][fd=3]{argument.sh}\echo{literal; HOME})",file),"literal; HOME");
    // The same source is read from the defining file's directory even when the
    // command is invoked later from a different included file / REPL fragment.
    engine.compileFragment(R"(\externcommand{\echo}[1][fd=3]{argument.sh})",file);
    output(engine.compileFragment(R"(\echo{kept})",dir/"other"/"source.tex"),"kept");
    output(engine.compile(R"(\def\a{outer}{\mmlcommand{\a}{"inner"}\a}\a)"),"innerouter");
    output(engine.compile(R"(\mmlcommand{\a}{"outer"}{\def\a{inner}\a}\a)"),"innerouter");
    output(engine.compile(R"(\mmlcommand{\a}{"outer"}{\gdef\a{global}\a}\a)"),"globalglobal");
    CHECK(failed(engine.compile(R"(\mmlcommand{\bad}{1 + "x"}\bad)")));
    CHECK(failed(engine.compile(R"(\mmlcommand{\bad}[10]{1}\bad)")));
    CHECK(failed(engine.compile(R"(\Extern[fd=0]{emit.sh})",file)));
    CHECK(failed(engine.compile(R"(\Extern{missing.sh})",file)));
    writeFile(dir/"fail.sh","#!/bin/sh\nprintf 'must not be emitted'\nexit 7\n");
    std::filesystem::permissions(dir/"fail.sh",std::filesystem::perms::owner_all);
    auto failedDoc=engine.compile(R"(\Extern{fail.sh})",file);CHECK(failed(failedDoc));CHECK(failedDoc.blocks.empty());
    CHECK(failedDoc.diagnostics[0].message.find("status 7")!=std::string::npos);
    engine.registerCommand("section",[](Engine& e){e.heading(1);});
    auto first=engine.compileFragment(R"(\section{First})");
    auto second=engine.compileFragment(R"(\section{Second})");
    CHECK(first.blocks[0].id!=second.blocks[0].id);
    std::cout<<"Scripting primitive tests passed\n";
}
